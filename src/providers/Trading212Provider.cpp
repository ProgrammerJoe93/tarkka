#include "providers/Trading212Provider.h"

#include "net/HttpClient.h"
#include "providers/CsvParser.h"
#include "providers/Trading212Csv.h"

#include <QDateTime>
#include <QTimeZone>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

namespace {

constexpr auto kExportsPath = "/api/v0/equity/history/exports";
/*
 * Listing reports is documented at one request per minute, and requesting one
 * at one per thirty seconds. Polling faster does not get the report sooner, it
 * just spends the whole budget on 429s, so the interval sits just above the
 * documented window.
 */
constexpr int kPollIntervalMs = 65000;
constexpr int kMaxPolls = 10;  //!< Roughly eleven minutes.

QString reportStatus(const QJsonObject &report)
{
    return report.value(QStringLiteral("status")).toString();
}

//! The API has used both spellings across revisions.
QString downloadLink(const QJsonObject &report)
{
    const QString link = report.value(QStringLiteral("downloadLink")).toString();
    return link.isEmpty() ? report.value(QStringLiteral("download_link")).toString() : link;
}

int reportId(const QJsonObject &report)
{
    const QJsonValue value = report.contains(QStringLiteral("reportId"))
                                 ? report.value(QStringLiteral("reportId"))
                                 : report.value(QStringLiteral("report_id"));
    return value.toInt(-1);
}

} // namespace

Trading212Provider::Trading212Provider(QObject *parent)
    : Provider(parent)
    , m_http(new HttpClient(this))
    , m_downloader(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &Trading212Provider::pollForReport);
}

QByteArray Trading212Provider::basicAuthHeader(const QString &apiKey, const QString &apiSecret)
{
    const QByteArray credentials = apiKey.toUtf8() + ':' + apiSecret.toUtf8();
    return QByteArrayLiteral("Basic ") + credentials.toBase64();
}

bool Trading212Provider::isConfigured() const
{
    return m_settings.hasTrading212Credentials();
}

void Trading212Provider::fetch(const QDate &from, const QDate &to)
{
    if (m_busy) {
        emit progress(tr("Trading 212 export already in progress…"));
        return;
    }
    if (!isConfigured()) {
        emit failed(tr("Add your Trading 212 API key and secret in Settings first."));
        return;
    }

    m_http->setBaseUrl(QUrl(m_settings.trading212BaseUrl()));
    m_http->setAuthorizationHeader(basicAuthHeader(m_settings.trading212ApiKey(),
                                                   m_settings.trading212ApiSecret()));

    m_busy = true;
    m_reportId = -1;
    m_pollsRemaining = kMaxPolls;
    requestExport(from, to);
}

void Trading212Provider::requestExport(const QDate &from, const QDate &to)
{
    const QJsonObject body{
        {QStringLiteral("dataIncluded"),
         QJsonObject{{QStringLiteral("includeDividends"), true},
                     {QStringLiteral("includeInterest"), true},
                     // Trades are portfolio activity, not spending, and asking
                     // for them only makes the report slower to build.
                     {QStringLiteral("includeOrders"), false},
                     {QStringLiteral("includeTransactions"), true}}},
        {QStringLiteral("timeFrom"),
         QDateTime(from, QTime(0, 0), QTimeZone::UTC).toString(Qt::ISODate)},
        {QStringLiteral("timeTo"),
         QDateTime(to, QTime(23, 59, 59), QTimeZone::UTC).toString(Qt::ISODate)},
    };

    emit progress(tr("Asking Trading 212 for a statement export…"));

    m_http->post(QLatin1String(kExportsPath),
                 QJsonDocument(body),
                 [this](const HttpClient::Response &response) {
                     if (!response.ok) {
                         giveUp(tr("Trading 212 refused the export request: %1")
                                    .arg(response.errorMessage));
                         return;
                     }

                     m_reportId = reportId(response.body.object());
                     if (m_reportId < 0) {
                         giveUp(tr("Trading 212 did not return a report id."));
                         return;
                     }

                     emit progress(tr("Trading 212 is building the export. This takes a "
                                      "minute or two…"));
                     m_pollTimer->start();
                     // One immediate check, then once per rate-limit window.
                     pollForReport();
                 });
}

void Trading212Provider::pollForReport()
{
    if (--m_pollsRemaining < 0) {
        giveUp(tr("Trading 212 did not finish the export in time. It is still being built — "
                  "try syncing again shortly."));
        return;
    }

    m_http->get(QLatin1String(kExportsPath), {}, [this](const HttpClient::Response &response) {
        if (!response.ok) {
            // A rate limit reply is expected while polling; keep waiting.
            if (response.httpStatus == 429)
                return;
            giveUp(tr("Could not check the Trading 212 export: %1").arg(response.errorMessage));
            return;
        }

        const QJsonArray reports = response.body.isArray()
                                       ? response.body.array()
                                       : response.body.object()
                                             .value(QStringLiteral("items")).toArray();

        for (const QJsonValue &value : reports) {
            const QJsonObject report = value.toObject();
            if (reportId(report) != m_reportId)
                continue;

            const QString status = reportStatus(report);
            if (status.compare(QLatin1String("Finished"), Qt::CaseInsensitive) == 0) {
                const QString link = downloadLink(report);
                if (link.isEmpty()) {
                    giveUp(tr("Trading 212 finished the export but returned no download link."));
                    return;
                }
                m_pollTimer->stop();
                download(QUrl(link));
                return;
            }

            if (status.compare(QLatin1String("Failed"), Qt::CaseInsensitive) == 0
                || status.compare(QLatin1String("Canceled"), Qt::CaseInsensitive) == 0) {
                giveUp(tr("Trading 212 could not build the export (%1).").arg(status));
                return;
            }
            return;  // Still queued or running.
        }
    });
}

void Trading212Provider::download(const QUrl &link)
{
    emit progress(tr("Downloading the Trading 212 export…"));

    // The download link is pre-signed storage, so it must not carry the API key.
    QNetworkReply *reply = m_downloader->get(QNetworkRequest(link));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            giveUp(tr("Could not download the Trading 212 export: %1").arg(reply->errorString()));
            return;
        }

        QString errorMessage;
        const Csv::Table table = Csv::parse(Csv::decode(reply->readAll()));
        const TransactionList transactions = Trading212::parse(table, &errorMessage);

        m_busy = false;

        if (transactions.isEmpty()) {
            // An empty month is not an error worth shouting about.
            emit finished(errorMessage.isEmpty()
                              ? tr("Trading 212: nothing new in this period.")
                              : tr("Trading 212: %1").arg(errorMessage));
            return;
        }

        emit transactionsReady(transactions);
        emit finished(tr("Trading 212: %n row(s)", nullptr,
                         static_cast<int>(transactions.size())));
    });
}

void Trading212Provider::giveUp(const QString &message)
{
    m_pollTimer->stop();
    m_busy = false;
    emit failed(message);
}
