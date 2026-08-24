// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
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

    ListView {
        anchors.fill: parent
        clip: true
        model: StoreModel

        delegate: Rectangle {
            id: delegateRoot

            required property var checksum
            required property string matchTitle
            required property string mapName
            required property bool hasExternalPath
            required property var timestamp
            required property var teams

            readonly property int fieldColumnWidth: 220

            width: ListView.view.width
            height: Math.max(fieldColumn.height, teamsFlow.height) + 16
            color: "transparent"

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
                        text: delegateRoot.matchTitle
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
                        icon.source: delegateRoot.hasExternalPath ? "qrc:/qt/qml/KWLegionUI/ico/eye-svgrepo-com.png" : "qrc:/qt/qml/KWLegionUI/ico/eye-closed-svgrepo-com.png"
                        icon.width: 16
                        icon.height: 16

                        onClicked: StoreModel.toggleReplayExposed(delegateRoot.checksum)
                        padding: 10
                        implicitWidth: implicitContentWidth + leftPadding + rightPadding
                        implicitHeight: implicitContentHeight + topPadding + bottomPadding
                    }

                    Button {
                        icon.source: "qrc:/qt/qml/KWLegionUI/ico/save-floppy-svgrepo-com.png"
                        icon.width: 16
                        icon.height: 16

                        padding: 10
                        implicitWidth: implicitContentWidth + leftPadding + rightPadding
                        implicitHeight: implicitContentHeight + topPadding + bottomPadding
                    }
                }
            }

            Flow {
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

                        width: teamColumn.width + 16
                        height: teamColumn.height + 16
                        color: "transparent"
                        border.color: Theme.lightMode ? Theme.dark : Theme.light
                        radius: 4

                        Column {
                            id: teamColumn
                            anchors.centerIn: parent
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
                                        width: 20
                                        height: 20
                                        fillMode: Image.PreserveAspectFit
                                    }

                                    Label {
                                        text: playerDelegate.name
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
