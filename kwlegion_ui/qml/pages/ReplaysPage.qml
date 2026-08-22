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

            required property string mapName
            required property var timestamp
            required property var teams

            width: ListView.view.width
            height: content.height + 16
            color: "transparent"

            Column {
                id: content
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 8

                Row {
                    spacing: 16

                    Label {
                        text: delegateRoot.mapName
                        color: Theme.lightMode ? Theme.dark : Theme.light
                    }

                    Label {
                        text: delegateRoot.timestamp.toLocaleString(Qt.locale())
                        color: Theme.lightMode ? Theme.dark : Theme.light
                    }
                }

                Rectangle {
                    width: parent.width
                    height: teamsColumn.height + 16
                    color: "transparent"
                    border.color: Theme.lightMode ? Theme.dark : Theme.light
                    radius: 4

                    Column {
                        id: teamsColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            text: qsTr("Teams")
                            font.bold: true
                            color: Theme.lightMode ? Theme.dark : Theme.light
                        }

                        Row {
                            spacing: 24

                            // delegateRoot.teams is a QList<TeamModel*>; Repeater
                            // treats it as a JS array, one modelData per team.
                            Repeater {
                                model: delegateRoot.teams

                                delegate: Column {
                                    id: teamColumn

                                    required property QtObject modelData

                                    spacing: 4

                                    // Each team is itself a model of players.
                                    Repeater {
                                        model: teamColumn.modelData

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
    }
}
