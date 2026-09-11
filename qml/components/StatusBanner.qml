import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BudgetApp

Rectangle {
    id: banner

    required property string message
    required property bool isError

    signal dismissed()

    visible: message.length > 0
    implicitHeight: visible ? Math.max(40, row.implicitHeight + 16) : 0
    radius: Theme.radiusSmall
    color: isError ? Qt.alpha(Theme.negative, 0.12) : Theme.surface
    border.width: 1
    border.color: isError ? Qt.alpha(Theme.negative, 0.5) : Theme.border

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: 6
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: Theme.spacing

        Rectangle {
            Layout.preferredWidth: 6
            Layout.preferredHeight: 6
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 6
            radius: 3
            color: banner.isError ? Theme.negative : Theme.accent
        }

        Text {
            Layout.fillWidth: true
            text: banner.message
            color: banner.isError ? Theme.negative : Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        Button {
            id: dismissButton

            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignTop
            flat: true
            text: "✕"
            onClicked: banner.dismissed()

            Accessible.name: qsTr("Dismiss message")

            contentItem: Text {
                text: dismissButton.text
                color: dismissButton.hovered ? Theme.text : Theme.textFaint
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 4
                color: dismissButton.hovered ? Theme.surfaceHover : "transparent"
            }
        }
    }
}
