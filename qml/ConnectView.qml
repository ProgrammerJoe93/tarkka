pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Tarkka

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

                // Wraps for the same reason as the row below: the title plus
                // three actions do not fit a phone on one line, and in Finnish
                // they do not fit on rather more screens than that.
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    Text {
                        text: qsTr("Linked accounts")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                    }

                    Text {
                        visible: AppController.accounts.count > 0
                        text: qsTr("Export…")
                        color: Theme.accent
                        font.pixelSize: Theme.fontSizeSmall

                        TapHandler { onTapped: exportDialog.open() }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }

                    Text {
                        text: qsTr("Import…")
                        color: Theme.accent
                        font.pixelSize: Theme.fontSizeSmall

                        TapHandler { onTapped: importDialog.open() }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
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
                    text: qsTr("No accounts are linked yet. Pick your bank below to start the "
                               + "consent flow in your browser, or import connections exported "
                               + "from another device.")
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
                        required property string identifier
                        required property string currency
                        required property bool isCard

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

                        Rectangle {
                            visible: accountRow.isCard
                            Layout.preferredWidth: cardLabel.implicitWidth + 14
                            Layout.preferredHeight: 18
                            radius: 9
                            color: Qt.alpha(Theme.accent, 0.16)

                            Text {
                                id: cardLabel
                                anchors.centerIn: parent
                                text: qsTr("Credit card")
                                color: Theme.accent
                                font.pixelSize: 10
                            }
                        }

                        Text {
                            text: accountRow.identifier
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

                /*
                 * A Flow rather than a RowLayout: these three are fixed-width,
                 * and a row of them is wider than a phone. A RowLayout does not
                 * shrink past its children's implicit widths, so the last
                 * button simply ran off the side of the card. Wrapping costs a
                 * line on a narrow screen and nothing on a wide one.
                 */
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    TextField {
                        id: browseCountry

                        width: 64
                        height: 34
                        text: AppController.country
                        maximumLength: 2
                        color: Theme.text
                        selectByMouse: true

                        Accessible.name: qsTr("Country to list banks for")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Two-letter country code. Change it to link an "
                                           + "account in another country.")

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: Theme.background
                            border.width: 1
                            border.color: browseCountry.activeFocus ? Theme.accent : Theme.border
                        }
                    }

                    Button {
                        id: connectButton

                        text: AppController.banks.length > 0
                              ? qsTr("Refresh bank list") : qsTr("Load banks")
                        enabled: AppController.configured && !AppController.busy
                        onClicked: AppController.loadBanks(browseCountry.text)

                        contentItem: Text {
                            text: connectButton.text
                            color: connectButton.enabled ? Theme.textOnAccent : Theme.textFaint
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
                    text: qsTr("Works without any credentials. Export a CSV from your bank, "
                               + "or from Trading 212 (History → Export), and drop it in. "
                               + "Columns are matched by name, and rows already synced "
                               + "through an API are not duplicated.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                /*
                 * Wraps like the rows above. A Flow top-aligns what it places,
                 * so the two labels carry the controls' height and centre their
                 * text within it; without that they would ride the top of a
                 * 34-pixel row.
                 */
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    Text {
                        text: qsTr("From")
                        height: 34
                        verticalAlignment: Text.AlignVCenter
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    ComboBox {
                        id: importInstitution

                        width: 200
                        height: 34
                        editable: true
                        // Nordea and Revolut exports are recognised from their
                        // headers; the other Finnish banks share too much
                        // vocabulary to tell apart, so they are named here.
                        model: [qsTr("Detect automatically"), "S-Pankki", "Nordea", "Revolut",
                                "Trading 212", "OP", "Danske Bank", "Aktia",
                                "Säästöpankki", "Lloyds", "Monzo", "Starling",
                                "HSBC", "Barclays", "NatWest", "Santander"]

                        readonly property string institutionHint:
                            currentIndex === 0 && editText === textAt(0) ? "" : editText.trim()

                        contentItem: TextField {
                            text: importInstitution.editText
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                            rightPadding: 24
                            selectByMouse: true
                            background: null
                            onTextEdited: importInstitution.editText = text
                            // A value wider than the field leaves the cursor at
                            // its end, which scrolls the start out of view --
                            // Finnish "Tunnista automaattisesti" then reads as
                            // "nnista automaattisesti". Only while unfocused,
                            // so it does not fight someone typing.
                            onTextChanged: if (!activeFocus) cursorPosition = 0
                        }

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: Theme.background
                            border.width: 1
                            border.color: importInstitution.activeFocus ? Theme.accent
                                                                        : Theme.border
                        }
                    }

                    Text {
                        text: qsTr("in")
                        height: 34
                        verticalAlignment: Text.AlignVCenter
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    TextField {
                        id: importCurrency

                        width: 64
                        height: 34
                        text: "EUR"
                        maximumLength: 3
                        color: Theme.text
                        selectByMouse: true

                        Accessible.name: qsTr("Currency for files that do not state one")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Used only when the file names no currency. "
                                           + "Several UK exports do not.")

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: Theme.background
                            border.width: 1
                            border.color: importCurrency.activeFocus ? Theme.accent : Theme.border
                        }
                    }

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
        id: exportDialog

        title: qsTr("Save the bank connections")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        /*
         * Android's picker offers no filter selector, so the first name filter
         * becomes a MIME filter for the whole dialog and everything outside it
         * is greyed out rather than merely deprioritised. Desktop keeps the
         * filter.
         */
        nameFilters: Qt.platform.os === "android"
                     ? [qsTr("All files (*)")]
                     : [qsTr("Tarkka connections (*.json)"), qsTr("All files (*)")]
        onAccepted: AppController.exportConnection(selectedFile)
    }

    FileDialog {
        id: importDialog

        title: qsTr("Choose exported bank connections")
        /*
         * Android's picker offers no filter selector, so the first name filter
         * becomes a MIME filter for the whole dialog and everything outside it
         * is greyed out rather than merely deprioritised. Desktop keeps the
         * filter.
         */
        nameFilters: Qt.platform.os === "android"
                     ? [qsTr("All files (*)")]
                     : [qsTr("Tarkka connections (*.json)"), qsTr("All files (*)")]
        onAccepted: AppController.importConnection(selectedFile)
    }

    FileDialog {
        id: fileDialog

        title: qsTr("Choose a bank statement")
        /*
         * Android's picker offers no filter selector, so the first name filter
         * becomes a MIME filter for the whole dialog and everything outside it
         * is greyed out rather than merely deprioritised. Neither .pem nor
         * .tsv has a MIME type the document providers agree on -- a plain .csv
         * often arrives as application/octet-stream too -- so restricting here
         * makes the file unpickable, which pushes the user into typing a path
         * that scoped storage then refuses to open. Desktop keeps the filter.
         */
        nameFilters: Qt.platform.os === "android"
                     ? [qsTr("All files (*)")]
                     : [qsTr("Statement files (*.csv *.txt *.tsv)"), qsTr("All files (*)")]
        onAccepted: AppController.importStatement(selectedFile,
                                                  importInstitution.institutionHint,
                                                  importCurrency.text)
    }
}
