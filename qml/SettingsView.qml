import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import BudgetApp

ScrollView {
    id: view

    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: view.availableWidth
        spacing: Theme.spacingLarge

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: credentialsColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: credentialsColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                Text {
                    text: qsTr("Enable Banking credentials")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Register an application at enablebanking.com, upload the public " + "half of an RSA key pair, then link your own Nordea and Revolut " + "accounts to it. Only the path to the private key is stored here — " + "the key itself never leaves the file you point at.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.spacing
                    rowSpacing: Theme.spacing

                    Text {
                        text: qsTr("Application ID")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    TextField {
                        id: applicationIdField

                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        text: AppController.applicationId
                        placeholderText: qsTr("00000000-0000-0000-0000-000000000000")
                        color: Theme.text
                        placeholderTextColor: Theme.textFaint
                        selectByMouse: true

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: Theme.background
                            border.width: 1
                            border.color: applicationIdField.activeFocus ? Theme.accent
                                                                         : Theme.border
                        }
                    }

                    Text {
                        text: qsTr("Private key")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            id: keyPathField

                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            text: AppController.privateKeyPath
                            placeholderText: qsTr("/home/you/.config/budget/enablebanking.pem")
                            color: Theme.text
                            placeholderTextColor: Theme.textFaint
                            selectByMouse: true

                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: Theme.background
                                border.width: 1
                                border.color: keyPathField.activeFocus ? Theme.accent : Theme.border
                            }
                        }

                        Button {
                            id: settingsButton

                            text: qsTr("Browse…")
                            onClicked: keyDialog.open()

                            contentItem: Text {
                                text: settingsButton.text
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeSmall
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitHeight: 34
                                implicitWidth: 84
                                radius: Theme.radiusSmall
                                color: settingsButton.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                                border.width: 1
                                border.color: Theme.border
                            }
                        }
                    }

                    Text {
                        text: qsTr("Country")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    TextField {
                        id: countryField

                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 34
                        text: AppController.country
                        maximumLength: 2
                        color: Theme.text
                        selectByMouse: true

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: Theme.background
                            border.width: 1
                            border.color: countryField.activeFocus ? Theme.accent : Theme.border
                        }
                    }

                    Text {
                        text: qsTr("Redirect port")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing

                        SpinBox {
                            id: portBox

                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 34
                            from: 1024
                            to: 65535
                            value: AppController.redirectPort
                            editable: true

                            // A port is an identifier, not a quantity, so it
                            // must not pick up the locale's digit grouping.
                            textFromValue: value => value.toString()
                            valueFromText: text => parseInt(text, 10)

                            contentItem: TextInput {
                                text: portBox.textFromValue(portBox.value, portBox.locale)
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeSmall
                                horizontalAlignment: Qt.AlignHCenter
                                verticalAlignment: Qt.AlignVCenter
                                readOnly: !portBox.editable
                                validator: portBox.validator
                                onTextEdited: portBox.value = portBox.valueFromText(text,
                                                                                    portBox.locale)
                            }

                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: Theme.background
                                border.width: 1
                                border.color: Theme.border
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Whitelist this exact URL in the control panel: %1")
                                .arg(AppController.redirectUrl)
                            color: Theme.textFaint
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight

                            TapHandler {
                                onTapped: AppController.copyToClipboard(AppController.redirectUrl)
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                        }
                    }
                }

                Button {
                    id: settingsButton2

                    Layout.topMargin: 4
                    text: qsTr("Save")
                    onClicked: AppController.saveCredentials(applicationIdField.text,
                                                             keyPathField.text,
                                                             portBox.value,
                                                             countryField.text)

                    contentItem: Text {
                        text: settingsButton2.text
                        color: Theme.background
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        implicitHeight: 34
                        implicitWidth: 100
                        radius: Theme.radiusSmall
                        color: settingsButton2.down ? Qt.darker(Theme.accent, 1.2) : Theme.accent
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: aboutColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: aboutColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: 6

                Text {
                    text: qsTr("Categories")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Merchant matching rules live in categories.json next to the " + "database. Edit that file and restart to change how purchases are " + "grouped; a category you set by hand on a row always wins.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    FileDialog {
        id: keyDialog

        title: qsTr("Choose your private key")
        nameFilters: [qsTr("PEM keys (*.pem *.key)"), qsTr("All files (*)")]
        onAccepted: keyPathField.text = selectedFile
    }
}
