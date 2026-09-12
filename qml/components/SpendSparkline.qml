pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Tarkka

/*!
    Daily outgoing totals across the selected range.

    The series is computed in C++ and arrives as one entry per calendar day, so
    empty days keep their place and the bar spacing matches the month rather
    than the number of purchases.
*/
Item {
    id: root

    required property var series

    readonly property int barCount: series ? series.length : 0
    readonly property real gap: 2

    implicitHeight: 76

    Row {
        anchors.fill: parent
        spacing: root.gap

        Repeater {
            model: root.series

            delegate: Item {
                id: bar

                required property var modelData

                readonly property real share: modelData.share

                width: Math.max(2, (root.width - (root.barCount - 1) * root.gap)
                                   / Math.max(1, root.barCount))
                height: root.height

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: Math.max(1, (parent.height - 4) * bar.share)
                    radius: Math.min(width, height) / 2
                    color: bar.share > 0 ? Theme.accent : Theme.border
                    opacity: bar.share > 0 ? 0.5 + 0.5 * bar.share : 1

                    Behavior on height {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }
                }

                HoverHandler {
                    id: hover
                    cursorShape: Qt.PointingHandCursor
                }

                ToolTip.visible: hover.hovered && bar.share > 0
                ToolTip.text: qsTr("%1 · %2")
                    .arg(bar.modelData.date.toLocaleDateString(Qt.locale(), Locale.ShortFormat))
                    .arg(bar.modelData.amountText)
            }
        }
    }
}
