pragma ComponentBehavior: Bound

import QtQuick
import Tarkka

/*!
    A small navigation glyph drawn from primitives.

    The obvious approach -- a Unicode symbol in a Text -- renders as a tofu box
    on Android, whose system fonts do not carry the geometric shapes the desktop
    fonts do. Drawing them keeps the icons identical on both platforms and
    independent of whatever fonts a device happens to ship.
*/
Item {
    id: icon

    required property string kind
    property color color: Theme.textFaint

    implicitWidth: 14
    implicitHeight: 14

    Accessible.ignored: true

    // Dashboard: a four-pane grid.
    Grid {
        anchors.centerIn: parent
        visible: icon.kind === "dashboard"
        columns: 2
        spacing: 3

        Repeater {
            model: 4
            delegate: Rectangle {
                required property int index
                width: 5
                height: 5
                radius: 1
                color: icon.color
            }
        }
    }

    // Trends: three bars of rising height, as in a chart.
    Row {
        anchors.centerIn: parent
        visible: icon.kind === "trend"
        spacing: 2

        Repeater {
            model: [6, 10, 14]

            delegate: Rectangle {
                required property int modelData
                width: 3
                height: modelData
                // Bottom-aligned, so the bars rise from a common baseline
                // rather than being centred at different heights.
                anchors.bottom: parent.bottom
                radius: 1
                color: icon.color
            }
        }
    }

    // Transactions: stacked rows, as in a list.
    Column {
        anchors.centerIn: parent
        visible: icon.kind === "list"
        spacing: 3

        Repeater {
            model: 3
            delegate: Rectangle {
                required property int index
                width: 13
                height: 2
                radius: 1
                color: icon.color
            }
        }
    }

    // Banks: a card with a magnetic stripe.
    Item {
        anchors.centerIn: parent
        visible: icon.kind === "card"
        width: 14
        height: 11

        Rectangle {
            anchors.fill: parent
            radius: 2
            color: "transparent"
            border.width: 1.5
            border.color: icon.color
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 3
            height: 2
            color: icon.color
        }
    }

    // Settings: a dial.
    Item {
        anchors.centerIn: parent
        visible: icon.kind === "gear"
        width: 14
        height: 14

        Rectangle {
            anchors.centerIn: parent
            width: 11
            height: 11
            radius: 5.5
            color: "transparent"
            border.width: 1.5
            border.color: icon.color
        }

        Rectangle {
            anchors.centerIn: parent
            width: 3
            height: 3
            radius: 1.5
            color: icon.color
        }
    }
}
