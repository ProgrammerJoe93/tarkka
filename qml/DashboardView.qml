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
                spacing: 2

                Text {
                    text: AppController.rangeLabel
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("%n purchase(s) across %1", "", AppController.transactions.count)
                        .arg(AppController.institutions.length > 0
                             ? AppController.institutions.join(", ")
                             : qsTr("no connected banks yet"))
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
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
            columns: view.availableWidth > 720 ? 3 : 1
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
                label: qsTr("NET")
                value: AppController.summary.netText
                caption: AppController.summary.hasTransfers
                         ? qsTr("Excludes %1 moved between your accounts")
                           .arg(AppController.summary.transfersText)
                         : qsTr("Income minus spending")
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
                             ? qsTr("Connect Nordea and Revolut, or import a statement file, " + "to see your purchases.")
                             : qsTr("No spending was recorded in %1.").arg(AppController.rangeLabel)
                    actionText: AppController.institutions.length === 0 ? qsTr("Connect a bank") : ""
                    onActionTriggered: view.connectRequested()
                }
            }
        }
    }
}
