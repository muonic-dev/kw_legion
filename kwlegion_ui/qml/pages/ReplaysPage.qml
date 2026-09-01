// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import KWLegionUI
import KWLegionCore

Page {
    id: page
    // Required for replaysListView.focus below to mean anything
    focus: true

    // StackLayout hides other pages by setting their visible to false rather
    // than destroying them. We want the listview focus back on load
    onVisibleChanged: if (visible) {
        replaysListView.forceActiveFocus();
    }

    background: Rectangle {
        color: Theme.lightMode ? Theme.reallyLight : Theme.reallyDark
    }

    // Maps the LegionParser::Faction enum to its logo resource, both registered
    // via the module's RESOURCES/qmldir mechanism under qrc:/qt/qml/KWLegionUI.
    function factionIcon(faction) {
        switch (faction) {
        case Faction.GDI:
            return "qrc:/qt/qml/KWLegionUI/ico/CNCKW_GDI_Logo.png";
        case Faction.ST:
            return "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Steel_Talons_Logo.png";
        case Faction.ZOCOM:
            return "qrc:/qt/qml/KWLegionUI/ico/CNCKW_ZOCOM_Logo.png";
        case Faction.Nod:
            return "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Nod_Logo.png";
        case Faction.MoK:
            return "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Marked_of_Kane_Logo.png";
        case Faction.BH:
            return "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Black_Hand_Logo.png";
        case Faction.Scrin:
            return "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Scrin_Logo.png";
        case Faction.Reaper:
            return "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Reaper-17_Logo.png";
        case Faction.Traveler:
            return "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Traveler-59_Logo.png";
        // Unknown. We treat unknown as synonymous with Random
        default:
            return "qrc:/qt/qml/KWLegionUI/ico/dice-24-svgrepo-com.png";
        }
    }

    // Checksums of the replays currently passing the header's text filter,
    // in list order. Bulk selection actions are scoped to this set so they
    // never silently act on rows the user can't currently see.
    function visibleChecksums() {
        const checksums = [];
        for (let row = 0; row < sortedStoreModel.rowCount(); row++) {
            checksums.push(sortedStoreModel.data(sortedStoreModel.index(row, 0), StoreModel.ChecksumRole));
        }
        return checksums;
    }

    FileDialog {
        id: fileDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["C&C Kane's Wrath Replays (*.KWReplay)"]
        defaultSuffix: "KWReplay"
        onAccepted: {
            if (page.currentlySavingChecksum) {
                console.error("tried to save nothing");
                return;
            }
            StoreModel.saveReplayAs(page.currentlySavingChecksum, selectedFile);
            page.currentlySavingChecksum = null;
        }
    }

    FolderDialog {
        id: folderDialog
        onAccepted: {
            StoreModel.exportSelectedReplaysTo(folderDialog.selectedFolder);
        }
    }

    SortFilterProxyModel {
        id: sortedStoreModel
        sourceModel: StoreModel
        sortRole: StoreModel.TimestampRole
        sortOrder: Qt.DescendingOrder
        filterPredicate: row => filterField.text.length == 0 || row.matchTitle.includes(filterField.text) || row.teams.some(team => team.playerNames.some(name => name.includes(filterField.text)))
    }

    Shortcut {
        sequence: StandardKey.SelectAll
        onActivated: {
            StoreModel.clearSelected();
            StoreModel.extendReplaySelection(page.visibleChecksums());
        }
    }

    header: ToolBar {
        padding: 8

        contentItem: RowLayout {
            spacing: 8

            TextField {
                id: filterField
                Layout.fillWidth: true
                placeholderText: qsTr("Filter replays…")

                Keys.onEscapePressed: replaysListView.forceActiveFocus()
                onTextChanged: sortedStoreModel.refilter()
            }

            Button {
                id: selectionMenuButton

                contentItem: RowLayout {
                    spacing: 2
                    TintedIcon {
                        source: "qrc:/qt/qml/KWLegionUI/ico/check-all-svgrepo-com.svg"
                        sourceSize: Qt.size(16, 16)
                    }
                    TintedIcon {
                        source: "qrc:/qt/qml/KWLegionUI/ico/chevron-down-svgrepo-com.svg"
                        sourceSize: Qt.size(10, 10)
                    }
                }

                // TODO: Clicking this button again while the menu is open
                // should close it. CloseOnPressOutside closes the popup on
                // the press itself, before this button's own click fires, so
                // toggling based on the popup's state at click time just
                // reopens it; capturing state on Button.onPressed instead
                // didn't resolve it either. Revisit before going public.
                onClicked: selectionMenu.open()

                padding: 10
                implicitWidth: implicitContentWidth + leftPadding + rightPadding
                implicitHeight: implicitContentHeight + topPadding + bottomPadding

                Menu {
                    id: selectionMenu
                    y: selectionMenuButton.height

                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                    MenuItem {
                        text: qsTr("Select All")
                        icon.source: "qrc:/qt/qml/KWLegionUI/ico/checkbox-check-svgrepo-com.svg"
                        icon.color: Theme.lightMode ? Theme.dark : Theme.light
                        onTriggered: {
                            StoreModel.clearSelected();
                            StoreModel.extendReplaySelection(page.visibleChecksums());
                        }
                    }
                    MenuItem {
                        text: qsTr("Select None")
                        icon.source: "qrc:/qt/qml/KWLegionUI/ico/checkbox-unchecked-svgrepo-com.svg"
                        icon.color: Theme.lightMode ? Theme.dark : Theme.light
                        onTriggered: StoreModel.clearSelected()
                    }
                    MenuItem {
                        text: qsTr("Invert Selection")
                        icon.source: "qrc:/qt/qml/KWLegionUI/ico/checkbox-fill-svgrepo-com.svg"
                        icon.color: Theme.lightMode ? Theme.dark : Theme.light
                        onTriggered: StoreModel.invertSelection(page.visibleChecksums())
                    }
                }
            }

            Button {
                enabled: StoreModel.selectionCount > 0
                opacity: enabled ? 1 : 0.4
                Behavior on opacity {
                    ShortAnimation {}
                }

                contentItem: TintedIcon {
                    source: "qrc:/qt/qml/KWLegionUI/ico/link-horizontal-svgrepo-com.svg"
                    sourceSize: Qt.size(16, 16)
                }

                onClicked: {
                    StoreModel.restrictSelectionTo(page.visibleChecksums());
                    StoreModel.showSelectedReplays();
                }

                padding: 10
                implicitWidth: 16 + leftPadding + rightPadding
                implicitHeight: 16 + topPadding + bottomPadding
            }

            Button {
                enabled: StoreModel.selectionCount > 0
                opacity: enabled ? 1 : 0.4
                Behavior on opacity {
                    ShortAnimation {}
                }

                contentItem: TintedIcon {
                    source: "qrc:/qt/qml/KWLegionUI/ico/link-horizontal-off-svgrepo-com.svg"
                    sourceSize: Qt.size(16, 16)
                }

                onClicked: {
                    StoreModel.restrictSelectionTo(page.visibleChecksums());
                    StoreModel.hideSelectedReplays();
                }

                padding: 10
                implicitWidth: 16 + leftPadding + rightPadding
                implicitHeight: 16 + topPadding + bottomPadding
            }

            Button {
                enabled: StoreModel.selectionCount > 0
                opacity: enabled ? 1 : 0.4
                Behavior on opacity {
                    ShortAnimation {}
                }

                contentItem: TintedIcon {
                    source: "qrc:/qt/qml/KWLegionUI/ico/export-svgrepo-com.svg"
                    sourceSize: Qt.size(16, 16)
                }

                onClicked: {
                    StoreModel.restrictSelectionTo(page.visibleChecksums());
                    folderDialog.open();
                }

                padding: 10
                implicitWidth: 16 + leftPadding + rightPadding
                implicitHeight: 16 + topPadding + bottomPadding
            }
        }
    }

    // Utility property for tracking multi-step actions
    property int lastSelectedIndex: -1
    property var currentlySavingChecksum

    ListView {
        id: replaysListView
        model: sortedStoreModel
        anchors.fill: parent
        clip: true
        focus: true

        Keys.onEscapePressed: StoreModel.clearSelected()

        Keys.onPressed: event => {
            if (event.key === Qt.Key_PageDown) {
                contentY = Math.min(contentY + height * 0.95, contentHeight - height);
                event.accepted = true;
            } else if (event.key === Qt.Key_PageUp) {
                contentY = Math.max(contentY - height * 0.95, 0);
                event.accepted = true;
            }
        }

        ScrollBar.vertical: ScrollBar {
            id: replaysScrollBar
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 6
                radius: width / 2
                color: Theme.lightMode ? Theme.dark : Theme.light
            }
        }

        delegate: Rectangle {
            id: delegateRoot

            required property int index
            required property var checksum
            required property string matchTitle
            required property string mapName
            required property string patch
            required property bool hasExternalPath
            required property bool selected
            required property var timestamp
            required property var teams

            // The width of the left zone containing textual data
            readonly property int fieldColumnWidth: 220

            width: ListView.view.width - 6 // some slight padding for the scrollbar
            height: Math.max(fieldColumn.height, teamsFlow.height) + 16
            color: "transparent"

            Rectangle {
                anchors.fill: parent
                color: Theme.selectionTint
                visible: delegateRoot.selected
            }

            MouseArea {
                anchors.fill: parent
                onClicked: mouse => {
                    // TODO: Revisit selection behavior
                    // Known issues:
                    // * no way of deselecting with shift
                    // * the selected range is always extended
                    //
                    // Should revise before we start going public
                    if (mouse.modifiers & Qt.ControlModifier) {
                        if (StoreModel.toggleReplaySelected(delegateRoot.checksum)) {
                            page.lastSelectedIndex = delegateRoot.index;
                        } else {
                            page.lastSelectedIndex = -1;
                        }
                    } else if (mouse.modifiers & Qt.ShiftModifier) {
                        // Nothing has been selected yet do do the direct selection path
                        // Maybe this should do nothing
                        if (page.lastSelectedIndex < 0) {
                            StoreModel.clearSelected();
                            StoreModel.setReplaySelected(delegateRoot.checksum);
                            page.lastSelectedIndex = delegateRoot.index;
                        } else {
                            const low = Math.min(delegateRoot.index, page.lastSelectedIndex);
                            const high = Math.max(delegateRoot.index, page.lastSelectedIndex);
                            const checkums = [];
                            for (let row = low; row <= high; row++) {
                                checkums.push(sortedStoreModel.data(sortedStoreModel.index(row, 0), StoreModel.ChecksumRole));
                            }
                            page.lastSelectedIndex = delegateRoot.index;
                            StoreModel.extendReplaySelection(checkums);
                        }
                    } else {
                        page.lastSelectedIndex = delegateRoot.index;
                        StoreModel.clearSelected();
                        StoreModel.setReplaySelected(delegateRoot.checksum);
                    }
                }
            }

            Item {
                id: fieldColumn
                x: 8
                y: 8
                width: delegateRoot.fieldColumnWidth
                height: textColumn.height + 4 + actionBar.height

                Column {
                    id: textColumn
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 4

                    Label {
                        width: parent.width
                        text: delegateRoot.matchTitle
                        font.bold: true
                        elide: Text.ElideRight
                        color: Theme.lightMode ? Theme.dark : Theme.light
                    }

                    Label {
                        width: parent.width
                        text: delegateRoot.timestamp.toLocaleString(Qt.locale(), Locale.ShortFormat)
                        elide: Text.ElideRight
                        color: Theme.lightMode ? Theme.dark : Theme.light
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 8
                        Label {
                            Layout.fillWidth: true
                            text: delegateRoot.mapName
                            elide: Text.ElideRight
                            color: Theme.lightMode ? Theme.dark : Theme.light
                        }

                        PatchPill {
                            text: delegateRoot.patch
                        }
                    }
                }

                // There's a third (match statistics) button planned for here
                // eventually - two icons already come close to filling the
                // same vertical space a stacked layout would need, so a
                // horizontal row costs little extra height for a third.
                Row {
                    id: actionBar
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    spacing: 8

                    Button {
                        id: exposeButton

                        contentItem: Item {
                            implicitHeight: 16
                            implicitWidth: 16

                            TintedIcon {
                                id: linkOn
                                anchors.fill: parent
                                source: "qrc:/qt/qml/KWLegionUI/ico/link-horizontal-svgrepo-com.svg"
                                // Fix for blurry
                                sourceSize: Qt.size(width, height)
                                opacity: delegateRoot.hasExternalPath ? 1 : 0
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: 120
                                    }
                                }
                            }
                            TintedIcon {
                                id: linkOff
                                anchors.fill: parent
                                source: "qrc:/qt/qml/KWLegionUI/ico/link-horizontal-off-svgrepo-com.svg"
                                // Fix for blurry
                                sourceSize: Qt.size(width, height)
                                opacity: delegateRoot.hasExternalPath ? 0 : 1
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: 120
                                    }
                                }
                            }
                        }

                        onClicked: {
                            StoreModel.toggleReplayExposed(delegateRoot.checksum);
                        }

                        padding: 10
                        implicitWidth: implicitContentWidth + leftPadding + rightPadding
                        implicitHeight: implicitContentHeight + topPadding + bottomPadding
                    }

                    Button {
                        contentItem: TintedIcon {
                            source: "qrc:/qt/qml/KWLegionUI/ico/export-svgrepo-com.svg"
                            sourceSize: Qt.size(16, 16)
                        }

                        onClicked: {
                            page.currentlySavingChecksum = delegateRoot.checksum;
                            fileDialog.selectedFile = `${fileDialog.currentFolder}/${StoreModel.friendlySaveName(delegateRoot.checksum)}`;
                            fileDialog.open();
                        }

                        padding: 10
                        implicitWidth: 16 + leftPadding + rightPadding
                        implicitHeight: 16 + topPadding + bottomPadding
                    }
                }
            }

            Flow {
                id: teamsFlow
                x: fieldColumn.x + fieldColumn.width + 16
                y: 8
                width: delegateRoot.width - x - 8
                spacing: 12

                // Every team box is a fixed width, growing vertically as
                // players are added, rather than sized to fit a particular
                // match's team count. Most matches have small teams, and a
                // fixed width keeps every row's boxes aligned with each
                // other regardless of match type. Bigger teams (e.g. 4v4)
                // just end up as a taller single column; that's an accepted
                // trade for consistent alignment across the whole list.
                readonly property int teamBoxWidth: 140

                // delegateRoot.teams is a QList<TeamModel*>; Repeater
                // treats it as a JS array, one modelData per team.
                Repeater {
                    model: delegateRoot.teams

                    delegate: Rectangle {
                        id: teamBox

                        required property QtObject modelData
                        width: teamsFlow.teamBoxWidth
                        height: teamColumn.height + 16
                        color: "transparent"
                        border.color: Theme.lightMode ? Theme.dark : Theme.light
                        radius: 4

                        Column {
                            id: teamColumn
                            x: 8
                            y: 8
                            spacing: 4

                            // Each team is itself a model of players.
                            Repeater {
                                model: teamBox.modelData

                                delegate: Row {
                                    id: playerDelegate
                                    required property string name
                                    required property int faction

                                    spacing: 6

                                    Image {
                                        source: page.factionIcon(playerDelegate.faction)
                                        sourceSize: Qt.size(32, 32)
                                        width: 24
                                        height: 24
                                        fillMode: Image.PreserveAspectFit
                                    }

                                    Label {
                                        width: 94
                                        text: playerDelegate.name
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                        color: Theme.lightMode ? Theme.dark : Theme.light
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
