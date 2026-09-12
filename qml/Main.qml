pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Tarkka

ApplicationWindow {
    id: window

    readonly property var pages: [
        { title: qsTr("Dashboard"), glyph: "dashboard" },
        { title: qsTr("Transactions"), glyph: "list" },
        { title: qsTr("Banks"), glyph: "card" },
        { title: qsTr("Settings"), glyph: "gear" }
    ]

    /*!
        A phone cannot spare 208px for a permanent sidebar, so below this width
        navigation moves to a bottom bar and the content margins tighten.
    */
    readonly property bool narrow: width < 760
    readonly property int pageMargin: narrow ? 12 : Theme.contentMargin

    width: 1180
    height: 780
    // Only meaningful where the window manager can honour them; Android runs
    // fullscreen at whatever the device provides.
    minimumWidth: narrow ? 0 : 720
    minimumHeight: narrow ? 0 : 480
    visible: true
    title: qsTr("Tarkka")
    color: Theme.background

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 208
            Layout.fillHeight: true
            visible: !window.narrow
            color: Theme.surface

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.border
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 8
                    Layout.bottomMargin: Theme.spacingLarge
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        radius: 8
                        color: Theme.accent

                        Text {
                            anchors.centerIn: parent
                            text: "€"
                            color: Theme.background
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Tarkka")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.DemiBold
                    }
                }

                Repeater {
                    model: window.pages

                    delegate: NavButton {
                        required property int index
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        title: modelData.title
                        glyph: modelData.glyph
                        current: stack.currentIndex === index
                        onActivated: stack.currentIndex = index
                    }
                }

                Item { Layout.fillHeight: true }

                Button {
                    id: syncButton

                    Layout.fillWidth: true
                    Layout.bottomMargin: 4
                    text: AppController.busy ? qsTr("Syncing…") : qsTr("Sync now")
                    enabled: !AppController.busy
                    onClicked: AppController.syncNow()

                    contentItem: Text {
                        text: syncButton.text
                        color: syncButton.enabled ? Theme.background : Theme.textFaint
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        implicitHeight: 36
                        radius: Theme.radiusSmall
                        color: !syncButton.enabled ? Theme.surfaceRaised
                                                   : syncButton.down ? Qt.darker(Theme.accent, 1.2)
                                                                     : Theme.accent
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.margins: 4
                    text: AppController.accounts.count > 0
                          ? qsTr("%n account(s) linked", "", AppController.accounts.count)
                          : qsTr("No accounts linked")
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            spacing: 0

            // Compact header, shown only when the sidebar is gone.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                visible: window.narrow
                color: Theme.surface

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: window.pageMargin
                    anchors.rightMargin: window.pageMargin
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        radius: 7
                        color: Theme.accent

                        Text {
                            anchors.centerIn: parent
                            text: "€"
                            color: Theme.background
                            font.pixelSize: 14
                            font.weight: Font.Bold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Tarkka")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                    }

                    Button {
                        id: compactSync

                        Layout.preferredHeight: 30
                        text: AppController.busy ? qsTr("Syncing…") : qsTr("Sync")
                        enabled: !AppController.busy
                        onClicked: AppController.syncNow()

                        contentItem: Text {
                            text: compactSync.text
                            color: compactSync.enabled ? Theme.background : Theme.textFaint
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                            rightPadding: 12
                        }

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: !compactSync.enabled ? Theme.surfaceRaised
                                                        : compactSync.down
                                                          ? Qt.darker(Theme.accent, 1.2)
                                                          : Theme.accent
                        }
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.border
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                Layout.margins: window.pageMargin
                spacing: Theme.spacing

                StatusBanner {
                    Layout.fillWidth: true
                    message: AppController.status
                    isError: AppController.statusIsError
                    onDismissed: AppController.dismissStatus()
                }

                StackLayout {
                    id: stack

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    /*
                     * A StackLayout reports the widest page as its minimum, so
                     * the settings form was pushing every other screen off the
                     * side of a phone. The pages all scroll or wrap, so none of
                     * them actually needs a floor.
                     */
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 0
                    Layout.minimumHeight: 0
                    Layout.preferredHeight: 0
                    currentIndex: 0

                    DashboardView {
                        onTransactionsRequested: stack.currentIndex = 1
                        onConnectRequested: stack.currentIndex = 2
                        onCategorySelected: category => {
                            AppController.categoryFilter = category;
                            stack.currentIndex = 1;
                        }
                    }

                    TransactionsView {}

                    ConnectView {
                        onSettingsRequested: stack.currentIndex = 3
                    }

                    SettingsView {}
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 58
                visible: window.narrow
                color: Theme.surface

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 4

                    Repeater {
                        model: window.pages

                        delegate: NavButton {
                            required property int index
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            stacked: true
                            title: modelData.title
                            glyph: modelData.glyph
                            current: stack.currentIndex === index
                            onActivated: stack.currentIndex = index
                        }
                    }
                }
            }
        }
    }

    // A thin progress strip is less intrusive than a modal overlay for a sync
    // that the user can keep browsing through.
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        visible: AppController.busy
        color: "transparent"

        Rectangle {
            id: pulse

            width: parent.width * 0.28
            height: parent.height
            color: Theme.accent

            XAnimator on x {
                running: AppController.busy
                loops: Animation.Infinite
                from: -pulse.width
                to: window.width
                duration: 1100
            }
        }
    }
}
