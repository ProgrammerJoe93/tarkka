import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Tarkka

ColumnLayout {
    id: root

    property string title: ""
    property string message: ""
    property string actionText: ""

    signal actionTriggered()

    spacing: Theme.spacing

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontSizeTitle
        font.weight: Font.DemiBold
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 420
        text: root.message
        color: Theme.textMuted
        font.pixelSize: Theme.fontSizeBody
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }

    Button {
        id: actionButton

        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 4
        visible: root.actionText.length > 0
        text: root.actionText
        onClicked: root.actionTriggered()

        contentItem: Text {
            text: actionButton.text
            color: Theme.background
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            implicitHeight: 36
            implicitWidth: 140
            radius: Theme.radiusSmall
            color: actionButton.down ? Qt.darker(Theme.accent, 1.2) : Theme.accent
        }
    }
}
