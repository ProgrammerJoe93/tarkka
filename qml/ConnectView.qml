pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import BudgetApp

ScrollView {
    id: view

    signal settingsRequested()

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: view.availableWidth
        spacing: Theme.spacingLarge

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: linkedColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: linkedColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Linked accounts")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                    }

                    Text {
                        visible: AppController.accounts.count > 0
                        text: qsTr("Disconnect all")
                        color: Theme.negative
                        font.pixelSize: Theme.fontSizeSmall

                        TapHandler { onTapped: AppController.disconnectAll() }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: AppController.accounts.count === 0
                    text: qsTr("No accounts are linked yet. Pick your bank below to start the " + "consent flow in your browser.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: AppController.accounts

                    delegate: RowLayout {
                        id: accountRow

                        required property string institution
                        required property string name
                        required property string iban
                        required property string currency

                        Layout.fillWidth: true
                        spacing: Theme.spacing

                        Rectangle {
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: Theme.positive
                        }

                        Text {
                            Layout.preferredWidth: 120
                            text: accountRow.institution
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeBody
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: accountRow.name
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }

                        Text {
                            text: accountRow.iban
                            color: Theme.textFaint
                            font.pixelSize: Theme.fontSizeSmall
                            font.family: "monospace"
                        }

                        Text {
                            text: accountRow.currency
                            color: Theme.textFaint
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: bankColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: bankColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                Text {
                    text: qsTr("Add a bank")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    visible: !AppController.configured
                    text: qsTr("Add your Enable Banking application id and private key first — " + "that is what authorises this app to read your own accounts.")
                    color: Theme.warning
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    Button {
                        id: connectButton

                        text: AppController.banks.length > 0
                              ? qsTr("Refresh bank list") : qsTr("Load banks for %1")
                                .arg(AppController.country)
                        enabled: AppController.configured && !AppController.busy
                        onClicked: AppController.loadBanks()

                        contentItem: Text {
                            text: connectButton.text
                            color: connectButton.enabled ? Theme.background : Theme.textFaint
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 34
                            implicitWidth: 170
                            radius: Theme.radiusSmall
                            color: !connectButton.enabled ? Theme.surfaceRaised
                                                   : connectButton.down ? Qt.darker(Theme.accent, 1.2)
                                                                 : Theme.accent
                        }
                    }

                    Button {
                        id: connectButton2

                        text: qsTr("Settings")
                        onClicked: view.settingsRequested()

                        contentItem: Text {
                            text: connectButton2.text
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 34
                            implicitWidth: 90
                            radius: Theme.radiusSmall
                            color: connectButton2.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                            border.width: 1
                            border.color: Theme.border
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                TextField {
                    id: bankSearch

                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    visible: AppController.banks.length > 0
                    placeholderText: qsTr("Filter banks…")
                    color: Theme.text
                    placeholderTextColor: Theme.textFaint
                    selectByMouse: true

                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: Theme.background
                        border.width: 1
                        border.color: bankSearch.activeFocus ? Theme.accent : Theme.border
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: AppController.banks

                        delegate: Rectangle {
                            id: bankChip

                            required property var modelData

                            readonly property bool matchesFilter:
                                bankSearch.text.length === 0
                                || modelData.name.toLowerCase()
                                   .indexOf(bankSearch.text.toLowerCase()) >= 0

                            visible: matchesFilter
                            width: visible ? chipLabel.implicitWidth + 28 : 0
                            height: visible ? 34 : 0
                            radius: 17
                            color: chipHover.hovered ? Theme.accentSoft : Theme.surfaceRaised
                            border.width: 1
                            border.color: chipHover.hovered ? Theme.accent : Theme.border

                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Connect %1").arg(bankChip.modelData.name)

                            Text {
                                id: chipLabel
                                anchors.centerIn: parent
                                text: bankChip.modelData.name
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            HoverHandler {
                                id: chipHover
                                cursorShape: Qt.PointingHandCursor
                            }

                            TapHandler {
                                onTapped: AppController.connectBank(bankChip.modelData.name,
                                                                    bankChip.modelData.country)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: importColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: importColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                Text {
                    text: qsTr("Import a statement")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Works without any credentials. Export a CSV from Nordea's " + "netbank or from Revolut (Statement → Excel/CSV) and drop it in — " + "the columns are matched by name, and rows already synced from " + "the API are not duplicated.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                Button {
                    id: connectButton3

                    text: qsTr("Choose CSV file…")
                    onClicked: fileDialog.open()

                    contentItem: Text {
                        text: connectButton3.text
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        implicitHeight: 34
                        implicitWidth: 150
                        radius: Theme.radiusSmall
                        color: connectButton3.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                        border.width: 1
                        border.color: Theme.border
                    }
                }
            }
        }
    }

    FileDialog {
        id: fileDialog

        title: qsTr("Choose a bank statement")
        nameFilters: [qsTr("Statement files (*.csv *.txt *.tsv)"), qsTr("All files (*)")]
        onAccepted: AppController.importStatement(selectedFile)
    }
}
