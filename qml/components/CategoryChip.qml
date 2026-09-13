import QtQuick
import Tarkka

/*!
    A transaction's category, coloured to match it everywhere else it appears,
    and tappable to change it.

    Extracted because the wide and compact row layouts place it differently but
    it must exist in both: it is the only route to the category editor, so a
    layout that drops it makes categories uneditable rather than merely less
    visible.
*/
Rectangle {
    id: chip

    required property string category
    //! Long category names elide rather than crowding out the row beside them.
    property int maximumWidth: 160

    signal tapped()

    implicitWidth: Math.min(label.implicitWidth + 16, maximumWidth)
    implicitHeight: 22
    radius: 11
    color: Qt.alpha(Theme.colorForCategory(chip.category), 0.16)

    Text {
        id: label

        anchors.centerIn: parent
        width: Math.min(implicitWidth, chip.maximumWidth - 16)
        text: chip.category
        color: Theme.colorForCategory(chip.category)
        font.pixelSize: Theme.fontSizeSmall
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
    }

    TapHandler {
        onTapped: chip.tapped()
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
