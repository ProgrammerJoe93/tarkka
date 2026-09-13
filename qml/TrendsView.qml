pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Tarkka

/*!
    Spending compared across the months leading up to the one on show.

    The dashboard answers where this month's money went; this answers whether
    that is more or less than usual, which is a different question and needs
    more than one month on screen at once.
*/
ScrollView {
    id: view

    contentWidth: availableWidth
    clip: true

    //! Below this the per-category month bars are dropped for the change alone.
    readonly property bool compact: availableWidth < 620

    ColumnLayout {
        width: view.availableWidth
        spacing: Theme.spacingLarge

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: totalsColumn.implicitHeight + 2 * Theme.spacingLarge
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: totalsColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: qsTr("Spending by month")
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeBody
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("The months up to the one the dashboard is showing.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                        }
                    }

                    ColumnLayout {
                        spacing: 2

                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: AppController.trends.latestTotalText
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeTitle
                            font.weight: Font.DemiBold
                        }

                        ChangeBadge {
                            Layout.alignment: Qt.AlignRight
                            text: AppController.trends.totalChangeText
                            direction: AppController.trends.totalChangeDirection
                        }
                    }
                }

                // One column per month, all scaled to the tallest, so the bars
                // are comparable with each other rather than each filling the
                // available height on its own.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 148
                    spacing: 6

                    Repeater {
                        model: AppController.trends.months

                        delegate: ColumnLayout {
                            id: monthColumn

                            required property var modelData
                            required property int index

                            readonly property bool latest:
                                index === AppController.trends.months.length - 1

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 4

                            Item { Layout.fillWidth: true; Layout.fillHeight: true }

                            Rectangle {
                                Layout.fillWidth: true
                                // A month with no spending still shows a sliver,
                                // so the column reads as empty rather than absent.
                                Layout.preferredHeight: Math.max(
                                    2, monthColumn.modelData.share * 104)
                                radius: Theme.radiusSmall
                                color: monthColumn.latest ? Theme.accent : Theme.accentSoft

                                ToolTip.visible: barHover.hovered
                                ToolTip.text: monthColumn.modelData.amountText

                                HoverHandler { id: barHover }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: monthColumn.modelData.label
                                color: monthColumn.latest ? Theme.text : Theme.textFaint
                                font.pixelSize: Theme.fontSizeSmall
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: categoriesColumn.implicitHeight + 2 * Theme.spacingLarge
            visible: AppController.trends.hasData
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: categoriesColumn

                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacing

                Text {
                    text: qsTr("By category")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }

                Repeater {
                    model: AppController.trends

                    delegate: RowLayout {
                        id: categoryRow

                        required property string category
                        required property var monthly
                        required property string latestText
                        required property string changeText
                        required property int changeDirection

                        Layout.fillWidth: true
                        spacing: Theme.spacing

                        Rectangle {
                            Layout.preferredWidth: 3
                            Layout.preferredHeight: 22
                            radius: 2
                            color: Theme.colorForCategory(categoryRow.category)
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 60
                            text: categoryRow.category
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }

                        // Each category is scaled against its own worst month,
                        // so a small category still shows its shape instead of
                        // a row of slivers next to a large one.
                        RowLayout {
                            visible: !view.compact
                            // Explicitly false: Layout.fillWidth defaults to
                            // true for a nested layout (only plain items
                            // default to false), so without this the bars
                            // absorb the row and squeeze the category name.
                            Layout.fillWidth: false
                            Layout.preferredWidth: visible ? 150 : 0
                            Layout.preferredHeight: 24
                            spacing: 3

                            Repeater {
                                model: categoryRow.monthly

                                delegate: Rectangle {
                                    required property var modelData

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.max(2, modelData.share * 22)
                                    Layout.alignment: Qt.AlignBottom
                                    radius: 2
                                    color: Qt.alpha(
                                        Theme.colorForCategory(categoryRow.category), 0.55)

                                    ToolTip.visible: cellHover.hovered
                                    ToolTip.text: modelData.amountText

                                    HoverHandler { id: cellHover }
                                }
                            }
                        }

                        Text {
                            Layout.preferredWidth: view.compact ? 84 : 100
                            text: categoryRow.latestText
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }

                        ChangeBadge {
                            Layout.preferredWidth: 62
                            text: categoryRow.changeText
                            direction: categoryRow.changeDirection
                        }
                    }
                }
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingLarge
            visible: !AppController.trends.hasData
            title: qsTr("Nothing to compare yet")
            message: qsTr("Once there is spending in more than one month, this is where the "
                          + "months line up against each other.")
        }
    }
}
