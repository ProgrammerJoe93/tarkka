import QtQuick
import QtQuick.Layouts
import Tarkka

/*!
    One entry in the navigation, in either of the two arrangements the window
    uses: a labelled row in the desktop sidebar, or an icon over a caption in
    the bottom bar a phone gets.
*/
Rectangle {
    id: button

    required property string title
    required property string glyph  //!< NavIcon kind, not a character.
    required property bool current
    property bool stacked: false

    signal activated()

    radius: Theme.radiusSmall
    color: current ? Theme.accentSoft : hover.hovered ? Theme.surfaceHover : "transparent"

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.checked: current

    GridLayout {
        anchors.fill: parent
        anchors.leftMargin: button.stacked ? 0 : 12
        anchors.rightMargin: button.stacked ? 0 : 12
        columns: button.stacked ? 1 : 2
        rowSpacing: 2
        columnSpacing: 10

        NavIcon {
            Layout.alignment: button.stacked ? Qt.AlignHCenter | Qt.AlignBottom
                                             : Qt.AlignVCenter
            kind: button.glyph
            color: button.current ? Theme.accent : Theme.textFaint
        }

        Text {
            Layout.fillWidth: true
            Layout.alignment: button.stacked ? Qt.AlignHCenter | Qt.AlignTop : Qt.AlignVCenter
            text: button.title
            color: button.current ? Theme.text : Theme.textMuted
            font.pixelSize: button.stacked ? 10 : Theme.fontSizeBody
            horizontalAlignment: button.stacked ? Text.AlignHCenter : Text.AlignLeft
            elide: Text.ElideRight
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: button.activated()
    }
}
