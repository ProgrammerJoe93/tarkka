import QtQuick
import QtQuick.Layouts
import Tarkka

Rectangle {
    id: card

    property string label: ""
    property string value: ""
    property string caption: ""
    property color valueColor: Theme.text

    implicitHeight: content.implicitHeight + 2 * Theme.spacingLarge
    radius: Theme.radius
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: 6

        Text {
            Layout.fillWidth: true
            text: card.label
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            font.letterSpacing: 0.6
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: card.value
            color: card.valueColor
            font.pixelSize: Theme.fontSizeDisplay
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            visible: card.caption.length > 0
            text: card.caption
            color: Theme.textFaint
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        // Cards in a row share a height; without this the ones that have no
        // caption would centre their content and sit out of line with the rest.
        Item { Layout.fillHeight: true }
    }
}
