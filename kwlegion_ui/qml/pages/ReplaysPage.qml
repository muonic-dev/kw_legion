// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import KWLegionUI
import KWLegionCore

Page {
    background: Rectangle {
        color: Theme.lightMode ? Theme.reallyLight : Theme.reallyDark
    }

    ListView {
        anchors.fill: parent
        clip: true
        model: StoreModel

        delegate: Rectangle {
            id: delegateRoot

            required property string mapName
            required property var timestamp

            width: ListView.view.width
            height: 40
            color: "transparent"

            Row {
                anchors.fill: parent
                anchors.margins: 8
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
        }
    }
}
