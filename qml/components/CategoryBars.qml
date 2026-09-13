pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Tarkka

ColumnLayout {
    id: root

    required property var model
    property int maximumRows: 8
    /*!
        Whether these are amounts coming in rather than going out.

        Only the wording changes: "purchases" and "percent of spending" are
        wrong for a salary, and a screen reader announcing either would be
        actively misleading about which way the money moved.
    */
    property bool incoming: false

    signal categoryActivated(string category)

    spacing: Theme.spacing

    Repeater {
        model: root.model

        delegate: Item {
            id: row

            required property int index
            required property string category
            required property string amountText
            required property real share
            required property int transactionCount

            readonly property color barColor: Theme.colorForCategory(row.category)

            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 44 : 0
            visible: row.index < root.maximumRows

            Accessible.role: Accessible.Button
            Accessible.name: root.incoming
                ? qsTr("%1, %2, %3 percent of income")
                    .arg(row.category).arg(row.amountText).arg(Math.round(row.share * 100))
                : qsTr("%1, %2, %3 percent of spending")
                    .arg(row.category).arg(row.amountText).arg(Math.round(row.share * 100))

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 4
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: row.barColor
                    }

                    Text {
                        Layout.fillWidth: true
                        text: row.category
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        elide: Text.ElideRight
                    }

                    Text {
                        text: root.incoming
                              ? qsTr("%n payment(s)", "", row.transactionCount)
                              : qsTr("%n purchase(s)", "", row.transactionCount)
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Text {
                        text: row.amountText
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: 3
                    color: Theme.surfaceRaised

                    Rectangle {
                        width: parent.width * Math.max(0.02, row.share)
                        height: parent.height
                        radius: parent.radius
                        color: row.barColor

                        Behavior on width {
                            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                        }
                    }
                }
            }

            TapHandler {
                onTapped: root.categoryActivated(row.category)
            }

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
        }
    }
}
