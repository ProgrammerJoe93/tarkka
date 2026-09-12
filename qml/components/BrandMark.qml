pragma ComponentBehavior: Bound

import QtQuick
import Tarkka

/*!
    The Tarkka mark, drawn rather than loaded.

    It is two rectangles, so drawing it costs less than decoding an image and it
    scales to any size without a second asset. assets/logo.svg is the same
    geometry and stays the source of truth for the launcher icons.
*/
Item {
    id: brand

    property color tileColor: Theme.accent
    property color markColor: Theme.textOnAccent
    property bool tile: true

    implicitWidth: 26
    implicitHeight: 26

    Accessible.ignored: true

    Rectangle {
        anchors.fill: parent
        visible: brand.tile
        radius: width * 0.223  // matches the 114/512 corner of the icon
        color: brand.tileColor
    }

    // Proportions taken straight from assets/logo.svg so the in-app mark and
    // the launcher icon cannot drift apart.
    Rectangle {
        x: parent.width * 0.207
        y: parent.height * 0.293
        width: parent.width * 0.586
        height: parent.height * 0.094
        radius: height / 2
        color: brand.markColor
    }

    Rectangle {
        x: parent.width * 0.453
        y: parent.height * 0.293
        width: parent.width * 0.094
        height: parent.height * 0.477
        radius: width / 2
        color: brand.markColor
    }
}
