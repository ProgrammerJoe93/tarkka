pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Tarkka

ScrollView {
    id: view

    signal categorySelected(string category)
    signal transactionsRequested()
    signal connectRequested()

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: view.availableWidth
        spacing: Theme.spacingLarge

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: AppController.rangeLabel
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    visible: AppController.summary.hasOtherCurrencies
                    text: qsTr("Showing %1 only — %n row(s) in other currencies are excluded",
                               "", AppController.summary.otherCurrencyCount)
                        .arg(AppController.summary.currency)
                    color: Theme.warning
                    font.pixelSize: Theme.fontSizeSmall
                }

                Text {
                    text: {
                        const where = AppController.institutions.length > 0
                                    ? AppController.institutions.join(", ")
                                    : qsTr("no connected banks yet");
                        const base = qsTr("%n purchase(s) across %1", "",
                                          AppController.transactions.count).arg(where);
                        const others = AppController.currencies.filter(
                            c => c !== AppController.currencyFilter);
                        return others.length > 0
                             ? qsTr("%1 · also holding %2").arg(base).arg(others.join(", "))
                             : base;
                    }
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }

            ComboBox {
                id: dashboardCurrency

                /*
                 * Totals only make sense within one currency, so the dashboard
                 * shows one at a time. Without this the other currencies would
                 * simply be missing from the figures with nothing to say so.
                 */
                visible: AppController.currencies.length > 1
                Layout.preferredWidth: visible ? 112 : 0
                Layout.preferredHeight: 32
                model: AppController.currencies
                currentIndex: Math.max(0, AppController.currencies
                                          .indexOf(AppController.currencyFilter))
                onActivated: index => AppController.currencyFilter =
                    AppController.currencies[index]

                Accessible.name: qsTr("Currency shown")

                contentItem: Text {
                    leftPadding: 10
                    rightPadding: 18
                    text: dashboardCurrency.displayText
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeSmall
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border
                }
            }

            ToolButton {
                id: monthButton

                text: "‹"
                onClicked: AppController.shiftMonth(-1)
                Accessible.name: qsTr("Previous month")

                contentItem: Text {
                    text: monthButton.text
                    color: Theme.text
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: 32
                    implicitHeight: 32
                    radius: Theme.radiusSmall
                    color: monthButton.hovered ? Theme.surfaceHover : Theme.surface
                    border.width: 1
                    border.color: Theme.border
                }
            }

            ToolButton {
                id: monthButton2

                text: "›"
                onClicked: AppController.shiftMonth(1)
                Accessible.name: qsTr("Next month")

                contentItem: Text {
                    text: monthButton2.text
                    color: Theme.text
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: 32
                    implicitHeight: 32
                    radius: Theme.radiusSmall
                    color: monthButton2.hovered ? Theme.surfaceHover : Theme.surface
                    border.width: 1
                    border.color: Theme.border
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            // Four cards when a balance is available, three otherwise; one per
            // row on a phone either way.
            columns: view.availableWidth > 720 ? (AppController.hasBalances ? 4 : 3) : 1
            columnSpacing: Theme.spacing
            rowSpacing: Theme.spacing

            SummaryCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: qsTr("SPENT")
                value: AppController.summary.totalSpentText
                caption: AppController.summary.largestCategory.length > 0
                         ? qsTr("Most on %1").arg(AppController.summary.largestCategory)
                         : ""
                valueColor: Theme.text
            }

            SummaryCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: qsTr("RECEIVED")
                value: AppController.summary.totalReceivedText
                valueColor: Theme.positive
            }

            SummaryCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                // Only when a bank actually served one: an account whose bank
                // does not report balances would otherwise show a confident
                // zero, which is worse than showing nothing.
                visible: AppController.hasBalances
                label: qsTr("IN ACCOUNTS")
                value: AppController.totalBalanceText
                caption: AppController.balancesInOtherCurrencies > 0
                         ? qsTr("%n account(s) in other currencies are not included", "",
                                AppController.balancesInOtherCurrencies)
                         : qsTr("As your banks last reported")
            }

            SummaryCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: qsTr("NET")
                value: AppController.summary.netText
                // Transfers and hand-excluded rows are both missing from the
                // figure, for different reasons, so the caption names whichever
                // applies rather than lumping them into one number the user
                // cannot account for.
                caption: {
                    const summary = AppController.summary;
                    if (summary.hasTransfers && summary.hasExcluded) {
                        return qsTr("Excludes %1 moved between your accounts and %2 you left out")
                            .arg(summary.transfersText).arg(summary.excludedText);
                    }
                    if (summary.hasTransfers) {
                        return qsTr("Excludes %1 moved between your accounts")
                            .arg(summary.transfersText);
                    }
                    if (summary.hasExcluded)
                        return qsTr("Excludes %1 you left out").arg(summary.excludedText);
                    return qsTr("Income minus spending");
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: sparkColumn.implicitHeight + 2 * Theme.spacingLarge
            visible: AppController.summary.dailySpend.length > 0
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: sparkColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                Text {
                    text: qsTr("Daily spending")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    font.letterSpacing: 0.6
                }

                SpendSparkline {
                    Layout.fillWidth: true
                    series: AppController.summary.dailySpend
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: categoryColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: categoryColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Where the money went")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        font.letterSpacing: 0.6
                    }

                    Text {
                        visible: AppController.summary.count > 0
                        text: qsTr("See all")
                        color: Theme.accent
                        font.pixelSize: Theme.fontSizeSmall

                        TapHandler { onTapped: view.transactionsRequested() }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                }

                CategoryBars {
                    Layout.fillWidth: true
                    visible: AppController.summary.count > 0
                    model: AppController.summary
                    onCategoryActivated: category => view.categorySelected(category)
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.spacing
                    Layout.bottomMargin: Theme.spacing
                    visible: AppController.summary.count === 0
                    title: qsTr("Nothing here yet")
                    message: AppController.institutions.length === 0
                             ? qsTr("Connect a bank account, or import a statement file, "
                                    + "to see your purchases.")
                             : qsTr("No spending was recorded in %1.").arg(AppController.rangeLabel)
                    actionText: AppController.institutions.length === 0 ? qsTr("Connect a bank") : ""
                    onActionTriggered: view.connectRequested()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: incomeColumn.implicitHeight + 2 * Theme.spacingLarge
            // Hidden rather than shown empty: most months have one salary and
            // nothing else, and a card saying so would be noise on every one of
            // them. A month with no income at all has nothing to say here.
            visible: AppController.summary.income.count > 0
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: incomeColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Where the money came from")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        font.letterSpacing: 0.6
                    }

                    Text {
                        text: qsTr("See all")
                        color: Theme.accent
                        font.pixelSize: Theme.fontSizeSmall

                        TapHandler { onTapped: view.transactionsRequested() }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                }

                CategoryBars {
                    Layout.fillWidth: true
                    model: AppController.summary.income
                    incoming: true
                    onCategoryActivated: category => view.categorySelected(category)
                }
            }
        }
    }
}
