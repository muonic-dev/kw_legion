// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import kw_legion

Page {
    id: root

    required property Main window

    background: Rectangle {
        color: root.window.lightMode ? root.window.reallyLight : root.window.reallyDark
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
                    color: root.window.lightMode ? root.window.dark : root.window.light
                }

                Label {
                    text: delegateRoot.timestamp.toLocaleString(Qt.locale())
                    color: root.window.lightMode ? root.window.dark : root.window.light
                }
            }
        }
    }
}
