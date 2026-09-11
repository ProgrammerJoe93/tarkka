#pragma once

#include "auth/Settings.h"
#include "providers/Provider.h"

#include <QDate>

class HttpClient;
class QNetworkAccessManager;
class QTimer;

/*!
 * Live access to a Trading 212 account through its own public API.
 *
 * Trading 212 is a broker, not a bank: it has no PSD2 interface, so the
 * aggregation API the banks share cannot reach it. Its public API needs only an
 * API key generated in the app.
 *
 * Card spending is fetched the long way round because the direct transactions
 * endpoint reports merchants as opaque UUIDs. Instead the provider asks for a
 * CSV export, polls until the report is built, downloads it and parses it with
 * the same code a manual import uses. That is slower than a single call, so
 * this provider is deliberately patient rather than chatty -- the export
 * endpoints are rate limited.
 */
class Trading212Provider : public Provider
{
    Q_OBJECT

public:
    explicit Trading212Provider(QObject *parent = nullptr);

    QString sourceId() const override { return QStringLiteral("csv"); }
    QString displayName() const override { return QStringLiteral("Trading 212"); }

    bool isConfigured() const;

    //! Requests an export for the range, then polls for it and parses it.
    void fetch(const QDate &from, const QDate &to);

private:
    void requestExport(const QDate &from, const QDate &to);
    void pollForReport();
    void download(const QUrl &link);
    void giveUp(const QString &message);

    Settings m_settings;
    HttpClient *m_http;
    QNetworkAccessManager *m_downloader;
    QTimer *m_pollTimer;

    int m_reportId = -1;
    int m_pollsRemaining = 0;
    bool m_busy = false;
};
