pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BudgetApp

ApplicationWindow {
    id: window

    readonly property var pages: [
        { title: qsTr("Dashboard"), glyph: "◴" },
        { title: qsTr("Transactions"), glyph: "≡" },
        { title: qsTr("Banks"), glyph: "⊞" },
        { title: qsTr("Settings"), glyph: "⚙" }
    ]

    width: 1180
    height: 780
    minimumWidth: 860
    minimumHeight: 560
    visible: true
    title: qsTr("Budget")
    color: Theme.background

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 208
            Layout.fillHeight: true
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
                        text: qsTr("Budget")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.DemiBold
                    }
                }

                Repeater {
                    model: window.pages

                    delegate: Rectangle {
                        id: navItem

                        required property int index
                        required property var modelData

                        readonly property bool current: stack.currentIndex === navItem.index

                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        radius: Theme.radiusSmall
                        color: navItem.current ? Theme.accentSoft
                                               : navHover.hovered ? Theme.surfaceHover
                                                                  : "transparent"

                        Accessible.role: Accessible.Button
                        Accessible.name: navItem.modelData.title

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            Text {
                                text: navItem.modelData.glyph
                                color: navItem.current ? Theme.accent : Theme.textFaint
                                font.pixelSize: 14
                            }

                            Text {
                                Layout.fillWidth: true
                                text: navItem.modelData.title
                                color: navItem.current ? Theme.text : Theme.textMuted
                                font.pixelSize: Theme.fontSizeBody
                                elide: Text.ElideRight
                            }
                        }

                        HoverHandler {
                            id: navHover
                            cursorShape: Qt.PointingHandCursor
                        }

                        TapHandler {
                            onTapped: stack.currentIndex = navItem.index
                        }
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
            Layout.margins: Theme.contentMargin
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
