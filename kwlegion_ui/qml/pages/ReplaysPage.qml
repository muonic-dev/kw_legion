// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Controls.Basic
import kw_legion

Page {
    background: Rectangle {
        color: window.lightMode ? window.reallyLight : window.reallyDark
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
                    color: window.lightMode ? window.dark : window.light
                }

                Label {
                    text: delegateRoot.timestamp.toLocaleString(Qt.locale())
                    color: window.lightMode ? window.dark : window.light
                }
            }
        }
    }
}
