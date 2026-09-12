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

    readonly property string primaryText: counterparty.length > 0 ? counterparty : description
    readonly property string secondaryText:
        counterparty.length > 0 && description.length > 0 && description !== counterparty
            ? description : accountName

    signal categoryEditRequested(string transactionId, string category)

    width: ListView.view ? ListView.view.width : implicitWidth
    height: 64

    //! Below this the bank column and the reference are dropped to keep the
    //! merchant and the amount legible on a phone.
    readonly property bool compact: width < 620

    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("%1, %2, %3, %4")
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

            Text {
                Layout.fillWidth: true
                visible: delegate.secondaryText.length > 0
                text: delegate.secondaryText
                color: Theme.textFaint
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideRight
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

        Rectangle {
            visible: !delegate.compact
            Layout.preferredWidth: visible ? categoryLabel.implicitWidth + 16 : 0
            Layout.preferredHeight: 22
            radius: 11
            color: Qt.alpha(Theme.colorForCategory(delegate.category), 0.16)

            Text {
                id: categoryLabel
                anchors.centerIn: parent
                text: delegate.category
                color: Theme.colorForCategory(delegate.category)
                font.pixelSize: Theme.fontSizeSmall
            }

            TapHandler {
                onTapped: delegate.categoryEditRequested(delegate.transactionId, delegate.category)
            }

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
        }

        Text {
            Layout.preferredWidth: delegate.compact ? 92 : 120
            text: delegate.amountText
            color: delegate.isSpend ? Theme.text : Theme.positive
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }
    }
}
