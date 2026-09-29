pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Tarkka

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

        GridLayout {
            id: filterBar

            // Wrap onto two rows rather than squeezing five controls across a
            // phone; the search field always takes the full first row there.
            readonly property bool narrow: view.width < 720

            Layout.fillWidth: true
            columns: narrow ? 2 : 5
            columnSpacing: Theme.spacing
            rowSpacing: Theme.spacing

            TextField {
                id: searchField

                Layout.fillWidth: true
                Layout.columnSpan: filterBar.narrow ? 2 : 1
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

                Layout.fillWidth: filterBar.narrow
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

                Layout.fillWidth: filterBar.narrow
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

            ComboBox {
                id: currencyFilter

                // Only meaningful with accounts in more than one currency.
                visible: AppController.currencies.length > 1
                Layout.fillWidth: filterBar.narrow
                Layout.preferredWidth: visible ? 112 : 0
                Layout.preferredHeight: 36
                model: AppController.currencies
                currentIndex: Math.max(0, AppController.currencies
                                          .indexOf(AppController.currencyFilter))
                onActivated: index => AppController.currencyFilter =
                    AppController.currencies[index]

                contentItem: Text {
                    leftPadding: 10
                    rightPadding: 24
                    text: currencyFilter.displayText
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeSmall
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.surface
                    border.width: 1
                    border.color: currencyFilter.activeFocus ? Theme.accent : Theme.border
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
                        color: Theme.textOnAccent
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            /*
             * A way in to the rows no rule claimed. They are invisible in a
             * total while quietly making every category wrong, so the count is
             * put where it will be noticed and one tap filters to them.
             */
            Rectangle {
                visible: AppController.uncategorisedCount > 0
                Layout.preferredWidth: visible ? uncategorisedLabel.implicitWidth + 20 : 0
                Layout.preferredHeight: 28
                radius: 14
                color: uncategorisedHover.hovered ? Qt.alpha(Theme.warning, 0.22)
                                                  : Qt.alpha(Theme.warning, 0.14)

                Accessible.role: Accessible.Button
                Accessible.name: uncategorisedLabel.text

                Text {
                    id: uncategorisedLabel

                    anchors.centerIn: parent
                    text: qsTr("%n uncategorised", "", AppController.uncategorisedCount)
                    color: Theme.warning
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                }

                TapHandler {
                    onTapped: AppController.categoryFilter = AppController.uncategorisedName()
                }

                HoverHandler {
                    id: uncategorisedHover
                    cursorShape: Qt.PointingHandCursor
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
                    onCategoryEditRequested: (transactionId, category, merchant) => {
                        categoryDialog.transactionId = transactionId;
                        categoryDialog.currentCategory = category;
                        categoryDialog.merchant = merchant;
                        categoryDialog.open();
                    }
                    onExcludeToggleRequested: (transactionId, excluded) =>
                        AppController.setTransactionExcluded(transactionId, excluded)
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
        //! The merchant a rule would match, from the row that was tapped.
        property string merchant: ""

        anchors.centerIn: Overlay.overlay
        width: 340
        modal: true
        title: qsTr("Change category")
        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            categoryInput.text = categoryDialog.currentCategory;
            alwaysBox.checked = false;
        }
        onAccepted: {
            const category = categoryInput.text.trim();
            if (category.length === 0)
                return;
            if (alwaysBox.checked && categoryDialog.merchant.length > 0) {
                // The rule covers this row too, so setting it individually as
                // well would only mark it hand-set and exempt it from the very
                // rule that was just asked for.
                AppController.addCategoryRule(categoryDialog.merchant, category);
            } else {
                AppController.setTransactionCategory(categoryDialog.transactionId, category);
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

            CheckBox {
                id: alwaysBox

                Layout.fillWidth: true
                visible: categoryDialog.merchant.length > 0
                text: qsTr("Always use this for “%1”").arg(categoryDialog.merchant)

                contentItem: Text {
                    leftPadding: alwaysBox.indicator.width + 8
                    text: alwaysBox.text
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeSmall
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                }

                indicator: Rectangle {
                    implicitWidth: 16
                    implicitHeight: 16
                    x: 0
                    y: (alwaysBox.height - height) / 2
                    radius: 4
                    color: alwaysBox.checked ? Theme.accent : Theme.surface
                    border.width: 1
                    border.color: alwaysBox.checked ? Theme.accent : Theme.borderStrong

                    Text {
                        anchors.centerIn: parent
                        visible: alwaysBox.checked
                        text: "✓"
                        color: Theme.textOnAccent
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: alwaysBox.checked
                text: qsTr("Writes a rule and applies it to matching transactions already "
                           + "stored. Rows you have categorised by hand keep what you gave them.")
                color: Theme.textFaint
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }
        }
    }
}
