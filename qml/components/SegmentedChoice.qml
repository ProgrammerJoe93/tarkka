pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Tarkka

/*!
    A row of buttons where exactly one is picked, for settings with a handful
    of fixed values.

    \a options are {value, label} pairs; \a label is what the button says and
    \a value is what comes back from \l chosen. Labels are passed in already
    translated, because some of them -- a language's own name, say -- must not
    be.
*/
RowLayout {
    id: choice

    required property var options
    required property string current

    signal chosen(string value)

    spacing: 8

    Repeater {
        model: choice.options

        delegate: Button {
            id: option

            required property var modelData

            readonly property bool picked: choice.current === modelData.value

            text: modelData.label
            Accessible.checked: picked
            onClicked: choice.chosen(modelData.value)

            contentItem: Text {
                text: option.text
                color: option.picked ? Theme.textOnAccent : Theme.text
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                implicitHeight: 34
                implicitWidth: 84
                radius: Theme.radiusSmall
                color: option.picked
                       ? (option.down ? Qt.darker(Theme.accent, 1.2) : Theme.accent)
                       : option.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                border.width: 1
                border.color: option.picked ? Theme.accent : Theme.border
            }
        }
    }
}
