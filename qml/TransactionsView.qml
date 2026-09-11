pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BudgetApp

Item {
    id: view

    function clearFilters() {
        AppController.transactions.searchText = "";
        AppController.categoryFilter = "";
        AppController.institutionFilter = "";
        AppController.spendOnly = false;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            TextField {
                id: searchField

                Layout.fillWidth: true
                Layout.preferredHeight: 36
                placeholderText: qsTr("Search merchants, references, categories…")
                text: AppController.transactions.searchText
                color: Theme.text
                placeholderTextColor: Theme.textFaint
                selectByMouse: true
                onTextEdited: AppController.transactions.searchText = text

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.surface
                    border.width: 1
                    border.color: searchField.activeFocus ? Theme.accent : Theme.border
                }
            }

            ComboBox {
                id: bankFilter

                Layout.preferredWidth: 150
                Layout.preferredHeight: 36
                model: [qsTr("All banks")].concat(AppController.institutions)
                currentIndex: AppController.institutionFilter === ""
                              ? 0
                              : Math.max(0, AppController.institutions
                                            .indexOf(AppController.institutionFilter) + 1)
                onActivated: index => AppController.institutionFilter =
                    index === 0 ? "" : AppController.institutions[index - 1]

                contentItem: Text {
                    leftPadding: 10
                    rightPadding: 24
                    text: bankFilter.displayText
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeSmall
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.surface
                    border.width: 1
                    border.color: bankFilter.activeFocus ? Theme.accent : Theme.border
                }
            }

            ComboBox {
                id: categoryFilter

                Layout.preferredWidth: 180
                Layout.preferredHeight: 36
                model: [qsTr("All categories")].concat(AppController.categories)
                currentIndex: AppController.categoryFilter === ""
                              ? 0
                              : Math.max(0, AppController.categories
                                            .indexOf(AppController.categoryFilter) + 1)
                onActivated: index => AppController.categoryFilter =
                    index === 0 ? "" : AppController.categories[index - 1]

                contentItem: Text {
                    leftPadding: 10
                    rightPadding: 24
                    text: categoryFilter.displayText
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeSmall
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.surface
                    border.width: 1
                    border.color: categoryFilter.activeFocus ? Theme.accent : Theme.border
                }
            }

            CheckBox {
                id: spendOnlyBox

                Layout.preferredHeight: 36
                text: qsTr("Purchases only")
                checked: AppController.spendOnly
                onToggled: AppController.spendOnly = checked

                contentItem: Text {
                    leftPadding: spendOnlyBox.indicator.width + 6
                    text: spendOnlyBox.text
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    verticalAlignment: Text.AlignVCenter
                }

                indicator: Rectangle {
                    implicitWidth: 16
                    implicitHeight: 16
                    x: 0
                    y: (spendOnlyBox.height - height) / 2
                    radius: 4
                    color: spendOnlyBox.checked ? Theme.accent : Theme.surface
                    border.width: 1
                    border.color: spendOnlyBox.checked ? Theme.accent : Theme.borderStrong

                    Text {
                        anchors.centerIn: parent
                        visible: spendOnlyBox.checked
                        text: "✓"
                        color: Theme.background
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border
            clip: true

            ListView {
                id: list

                anchors.fill: parent
                anchors.margins: 1
                model: AppController.transactions
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                visible: count > 0

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: TransactionDelegate {
                    onCategoryEditRequested: (transactionId, category) => {
                        categoryDialog.transactionId = transactionId;
                        categoryDialog.currentCategory = category;
                        categoryDialog.open();
                    }
                }
            }

            EmptyState {
                anchors.centerIn: parent
                width: Math.min(parent.width - 2 * Theme.contentMargin, 460)
                visible: list.count === 0
                title: qsTr("No matching purchases")
                message: qsTr("Try a wider date range, or clear the filters.")
                actionText: qsTr("Clear filters")
                onActionTriggered: view.clearFilters()
            }
        }
    }

    Dialog {
        id: categoryDialog

        property string transactionId: ""
        property string currentCategory: ""

        anchors.centerIn: Overlay.overlay
        width: 340
        modal: true
        title: qsTr("Change category")
        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: categoryInput.text = categoryDialog.currentCategory
        onAccepted: {
            if (categoryInput.text.trim().length > 0) {
                AppController.setTransactionCategory(categoryDialog.transactionId,
                                                     categoryInput.text.trim());
            }
        }

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surfaceRaised
            border.width: 1
            border.color: Theme.borderStrong
        }

        header: Text {
            padding: Theme.spacing
            text: categoryDialog.title
            color: Theme.text
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.DemiBold
        }

        contentItem: ColumnLayout {
            spacing: Theme.spacing

            Text {
                Layout.fillWidth: true
                text: qsTr("A category you set by hand is kept when this row is synced again.")
                color: Theme.textFaint
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            TextField {
                id: categoryInput

                Layout.fillWidth: true
                Layout.preferredHeight: 34
                color: Theme.text
                selectByMouse: true

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.surface
                    border.width: 1
                    border.color: categoryInput.activeFocus ? Theme.accent : Theme.border
                }
            }
        }
    }
}
