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
            description: qsTr("Start at login. LEGION needs to be running in order to track your replays.")
            checked: Settings.shouldAutostart
            onToggled: checked => Settings.shouldAutostart = checked
        }

        SettingsSwitchRow {
            name: qsTr("Start hidden")
            description: qsTr("Start hidden to the tray icon")
            checked: Settings.startMinimized
            onToggled: checked => Settings.startMinimized = checked
        }

        SettingsSwitchRow {
            name: qsTr("Minimize to tray")
            description: qsTr("Hide the window to the tray icon instead of quitting when you close close")
            checked: Settings.closeToTray
            onToggled: checked => Settings.closeToTray = checked
        }

        SettingsSwitchRow {
            name: qsTr("Automatically check for updates")
            description: qsTr("LEGION will check for updates periodically about once per day.")
            checked: Settings.checkForUpdates
            onToggled: checked => Settings.checkForUpdates = checked
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
