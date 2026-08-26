// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import KWLegionUI
import KWLegionCore

Page {
    id: page

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

    FileDialog {
        id: fileDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["C&C Kane's Wrath Replays (*.KWReplay)"]
        defaultSuffix: "KWReplay"
        onAccepted: {
            StoreModel.saveReplayAs(page.currentlySavingChecksum, selectedFile);
            page.currentlySavingChecksum = null;
        }
    }

    SortFilterProxyModel {
        id: sortedStoreModel
        sourceModel: StoreModel
        sortRole: StoreModel.TimestampRole
        sortOrder: Qt.DescendingOrder
    }

    Shortcut {
        sequence: StandardKey.SelectAll
        onActivated: StoreModel.selectAllReplays()
    }

    // Utility property for tracking multi-step actions
    property int lastSelectedIndex: -1
    property var currentlySavingChecksum

    ListView {
        anchors.fill: parent
        clip: true
        model: sortedStoreModel

        ScrollBar.vertical: ScrollBar {
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
                height: Math.max(textColumn.height + 4 + controlRow.height, teamsFlow.height)

                Column {
                    id: textColumn
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 4

                    Label {
                        width: parent.width
                        text: delegateRoot.patch.toUpperCase() + " " + delegateRoot.matchTitle
                        font.bold: true
                        elide: Text.ElideRight
                        color: Theme.lightMode ? Theme.dark : Theme.light
                    }

                    Label {
                        width: parent.width
                        text: delegateRoot.timestamp.toLocaleString(Qt.locale())
                        elide: Text.ElideRight
                        color: Theme.lightMode ? Theme.dark : Theme.light
                    }

                    Label {
                        width: parent.width
                        text: delegateRoot.mapName
                        elide: Text.ElideRight
                        color: Theme.lightMode ? Theme.dark : Theme.light
                    }
                }

                Row {
                    id: controlRow
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    spacing: 8

                    Button {
                        id: exposeButton

                        contentItem: Item {
                            implicitHeight: 16
                            implicitWidth: 16

                            Image {
                                id: eyeOpen
                                anchors.fill: parent
                                source: "qrc:/qt/qml/KWLegionUI/ico/eye-show-svgrepo-com.svg"
                                // Fix for blurry
                                sourceSize: Qt.size(width, height)
                                opacity: delegateRoot.hasExternalPath ? 1 : 0
                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: 120
                                    }
                                }
                            }
                            Image {
                                id: eyeClosed
                                anchors.fill: parent
                                source: "qrc:/qt/qml/KWLegionUI/ico/eye-hide-svgrepo-com.svg"
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
                        contentItem: Image {
                            source: "qrc:/qt/qml/KWLegionUI/ico/save-floppy-svgrepo-com.svg"
                            sourceSize: Qt.size(16, 16)
                            fillMode: Image.PreserveAspectFit
                        }

                        onClicked: {
                            page.currentlySavingChecksum = delegateRoot.checksum;
                            fileDialog.selectedFile = `${fileDialog.currentFolder}/${delegateRoot.matchTitle}`;
                            fileDialog.open();
                        }

                        padding: 10
                        implicitWidth: 16 + leftPadding + rightPadding
                        implicitHeight: 16 + topPadding + bottomPadding
                    }
                }
            }

            Column {
                id: teamsFlow
                x: fieldColumn.x + fieldColumn.width + 16
                y: 8
                width: delegateRoot.width - x - 8
                spacing: 12

                // delegateRoot.teams is a QList<TeamModel*>; Repeater
                // treats it as a JS array, one modelData per team.
                Repeater {
                    model: delegateRoot.teams

                    delegate: Rectangle {
                        id: teamBox

                        required property QtObject modelData
                        // TODO: There is some kind of rebuild/teardown that causes warnings unless this is guarded
                        width: parent ? parent.width : 0
                        height: teamColumn.height + 16
                        color: "transparent"
                        border.color: Theme.lightMode ? Theme.dark : Theme.light
                        radius: 4

                        Flow {
                            id: teamColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 8
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
                                        width: Math.min(120, teamColumn.width)
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
