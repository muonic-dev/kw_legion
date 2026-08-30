// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import KWLegionUI
import KWLegionCore

Page {
    background: Rectangle {
        color: Theme.lightMode ? Theme.reallyLight : Theme.reallyDark
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        SettingsSwitchRow {
            name: qsTr("Start automatically")
            description: qsTr("Start automatically when you sign in.")
            checked: Settings.shouldAutostart
            onCheckedChanged: Settings.shouldAutostart = checked
        }

        SettingsSwitchRow {
            name: qsTr("Start hidden")
            description: qsTr("Start hidden to the tray icon")
            checked: Settings.startMinimized
            onCheckedChanged: Settings.startMinimized = checked
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
