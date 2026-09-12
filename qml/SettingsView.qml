import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Tarkka

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
                    text: qsTr("Register an application at enablebanking.com, upload the public "
                               + "half of an RSA key pair, then link your own bank accounts to "
                               + "it. Only the path to the private key is stored here — the key "
                               + "itself never leaves the file you point at.")
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
                            placeholderText: qsTr("/home/you/.config/tarkka/enablebanking.pem")
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
                        color: Theme.textOnAccent
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
            Layout.preferredHeight: tradingColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: tradingColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Trading 212")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        visible: AppController.trading212Configured
                        Layout.preferredWidth: linkedLabel.implicitWidth + 16
                        Layout.preferredHeight: 20
                        radius: 10
                        color: Qt.alpha(Theme.positive, 0.16)

                        Text {
                            id: linkedLabel
                            anchors.centerIn: parent
                            text: qsTr("Connected")
                            color: Theme.positive
                            font.pixelSize: 10
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Trading 212 is a broker, not a bank, so it is reached through "
                               + "its own API rather than open banking. Generate a key pair in "
                               + "the app under Settings → API with history access; both halves "
                               + "are needed. They are written to an owner-only file, never "
                               + "into the settings.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    TextField {
                        id: trading212Key

                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        placeholderText: AppController.trading212Configured
                                         ? qsTr("API key — stored, retype to replace")
                                         : qsTr("API key")
                        color: Theme.text
                        placeholderTextColor: Theme.textFaint
                        selectByMouse: true

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: Theme.background
                            border.width: 1
                            border.color: trading212Key.activeFocus ? Theme.accent : Theme.border
                        }
                    }

                    TextField {
                        id: trading212Secret

                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        echoMode: TextInput.Password
                        placeholderText: qsTr("API secret")
                        color: Theme.text
                        placeholderTextColor: Theme.textFaint
                        selectByMouse: true

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: Theme.background
                            border.width: 1
                            border.color: trading212Secret.activeFocus ? Theme.accent : Theme.border
                        }
                    }

                    Button {
                        id: saveKeyButton

                        text: qsTr("Save")
                        enabled: trading212Key.text.trim().length > 0
                                 && trading212Secret.text.trim().length > 0
                        onClicked: {
                            AppController.saveTrading212Credentials(trading212Key.text,
                                                                    trading212Secret.text);
                            trading212Key.clear();
                            trading212Secret.clear();
                        }

                        contentItem: Text {
                            text: saveKeyButton.text
                            color: saveKeyButton.enabled ? Theme.textOnAccent : Theme.textFaint
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 34
                            implicitWidth: 96
                            radius: Theme.radiusSmall
                            color: !saveKeyButton.enabled ? Theme.surfaceRaised
                                                          : saveKeyButton.down
                                                            ? Qt.darker(Theme.accent, 1.2)
                                                            : Theme.accent
                        }
                    }

                    Button {
                        id: forgetKeyButton

                        visible: AppController.trading212Configured
                        text: qsTr("Forget")
                        onClicked: AppController.saveTrading212Credentials("", "")

                        contentItem: Text {
                            text: forgetKeyButton.text
                            color: Theme.negative
                            font.pixelSize: Theme.fontSizeSmall
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitHeight: 34
                            implicitWidth: 80
                            radius: Theme.radiusSmall
                            color: forgetKeyButton.hovered ? Theme.surfaceHover
                                                           : Theme.surfaceRaised
                            border.width: 1
                            border.color: Theme.border
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: appearanceColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: appearanceColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                Text {
                    text: qsTr("Appearance")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Follow the system and the app switches with it, the moment the "
                               + "desktop or phone does. Pick a palette instead and it stays "
                               + "put, here and on every later launch.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                SegmentedChoice {
                    options: [
                        { value: "system", label: qsTr("System") },
                        { value: "light", label: qsTr("Light") },
                        { value: "dark", label: qsTr("Dark") },
                    ]
                    current: AppController.appearance
                    onChosen: value => AppController.appearance = value
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: languageColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: languageColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                Text {
                    text: qsTr("Language")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Following the system picks the first of your preferred "
                               + "languages this app has been translated into. Categories and "
                               + "anything your bank wrote are left in the language they "
                               + "arrived in.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                SegmentedChoice {
                    // Each language is named in itself, so those two labels are
                    // deliberately not translated: someone who has landed in a
                    // language they cannot read needs to find their own by sight.
                    options: [
                        { value: "system", label: qsTr("System") },
                        { value: "en", label: "English" },
                        { value: "fi", label: "Suomi" },
                    ]
                    current: AppController.language
                    onChosen: value => AppController.language = value
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
