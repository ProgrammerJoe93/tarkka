import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Tarkka

ItemDelegate {
    id: delegate

    required property string transactionId
    required property string institution
    required property string accountName
    required property string dateText
    required property string description
    required property string counterparty
    required property string amountText
    required property string category
    required property bool isSpend
    required property bool isPending
    required property bool isExcluded

    readonly property string primaryText: counterparty.length > 0 ? counterparty : description
    readonly property string secondaryText:
        counterparty.length > 0 && description.length > 0 && description !== counterparty
            ? description : accountName

    signal categoryEditRequested(string transactionId, string category)
    signal excludeToggleRequested(string transactionId, bool excluded)

    width: ListView.view ? ListView.view.width : implicitWidth
    height: 64
    //! Gutter the exclude toggle sits in, so it never overlaps the amount.
    rightPadding: 32

    //! Below this the bank column and the reference are dropped to keep the
    //! merchant and the amount legible on a phone.
    readonly property bool compact: width < 620

    Accessible.role: Accessible.ListItem
    Accessible.name: delegate.isExcluded
        ? qsTr("%1, %2, %3, %4, left out of the totals")
            .arg(delegate.dateText).arg(delegate.primaryText)
            .arg(delegate.amountText).arg(delegate.category)
        : qsTr("%1, %2, %3, %4")
            .arg(delegate.dateText).arg(delegate.primaryText)
            .arg(delegate.amountText).arg(delegate.category)

    background: Rectangle {
        color: delegate.hovered ? Theme.surfaceHover : "transparent"

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.border
        }
    }

    contentItem: RowLayout {
        spacing: Theme.spacing
        // Reads as "still here, just not counted" at a glance, without having
        // to find and read the toggle at the end of the row.
        opacity: delegate.isExcluded ? 0.45 : 1

        Rectangle {
            Layout.preferredWidth: 3
            Layout.preferredHeight: 34
            radius: 2
            color: Theme.colorForCategory(delegate.category)
        }

        Text {
            Layout.preferredWidth: delegate.compact ? 52 : 74
            text: delegate.dateText
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideRight
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: delegate.primaryText
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeBody
                    elide: Text.ElideRight
                }

                Rectangle {
                    visible: delegate.isPending
                    Layout.preferredWidth: pendingLabel.implicitWidth + 12
                    Layout.preferredHeight: 18
                    radius: 9
                    color: Theme.surfaceRaised
                    border.width: 1
                    border.color: Theme.warning

                    Text {
                        id: pendingLabel
                        anchors.centerIn: parent
                        text: qsTr("Pending")
                        color: Theme.warning
                        font.pixelSize: 10
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                /*
                 * The compact layout drops the bank column and the wide row's
                 * category chip to keep the merchant and amount legible. The
                 * chip cannot simply go with them: tapping it is the only way
                 * to reach the category editor, so on a phone it moves down
                 * here rather than disappearing.
                 */
                CategoryChip {
                    visible: delegate.compact
                    Layout.preferredWidth: visible ? implicitWidth : 0
                    Layout.preferredHeight: 20
                    maximumWidth: 120
                    category: delegate.category
                    onTapped: delegate.categoryEditRequested(delegate.transactionId,
                                                             delegate.category)
                }

                Text {
                    Layout.fillWidth: true
                    visible: delegate.secondaryText.length > 0
                    text: delegate.secondaryText
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideRight
                }
            }
        }

        Text {
            visible: !delegate.compact
            Layout.preferredWidth: visible ? 96 : 0
            text: delegate.institution
            color: Theme.textFaint
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }

        CategoryChip {
            visible: !delegate.compact
            Layout.preferredWidth: visible ? implicitWidth : 0
            Layout.preferredHeight: 22
            category: delegate.category
            onTapped: delegate.categoryEditRequested(delegate.transactionId, delegate.category)
        }

        Text {
            Layout.preferredWidth: delegate.compact ? 92 : 120
            text: delegate.amountText
            color: delegate.isSpend ? Theme.text : Theme.positive
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.DemiBold
            font.strikeout: delegate.isExcluded
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }
    }

    /*
     * Outside contentItem so the dimming above does not apply to it: an
     * excluded row is faded, but the control that puts it back has to stay
     * legible.
     *
     * Shown on hover, always once the row is excluded so the state is never
     * invisible, and always in the compact layout -- a touch screen never
     * reports hover, so a hover-only control would leave the feature with no
     * way in at all on a phone.
     */
    Rectangle {
        id: excludeToggle

        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 22
        height: 22
        radius: 11
        visible: delegate.isExcluded || delegate.compact || delegate.hovered
                 || excludeHover.hovered
        color: excludeHover.hovered ? Theme.surfaceHover : "transparent"
        border.width: 1
        border.color: delegate.isExcluded ? Theme.warning : Theme.border

        Accessible.role: Accessible.Button
        Accessible.name: delegate.isExcluded ? qsTr("Include in the totals")
                                             : qsTr("Leave out of the totals")

        ToolTip.visible: excludeHover.hovered
        ToolTip.text: Accessible.name

        // Drawn rather than set as a glyph: the circle is already this
        // rectangle's border, and a bar across it needs no font to be present.
        Rectangle {
            anchors.centerIn: parent
            width: 12
            height: 1.5
            rotation: -45
            color: delegate.isExcluded ? Theme.warning : Theme.textFaint
        }

        HoverHandler {
            id: excludeHover
            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            onTapped: delegate.excludeToggleRequested(delegate.transactionId,
                                                      !delegate.isExcluded)
        }
    }
}
