// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.platform
import KWLegionUI
import "pages"

ApplicationWindow {
    id: appWindow
    width: 1100
    height: 700
    minimumWidth: 200
    minimumHeight: 250
    visible: true
    title: qsTr("LEGION Replay Manager")

    SystemTrayIcon {
        visible: true
        icon.source: Theme.appIcon
        tooltip: appWindow.title
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavRail {
            id: navRail
            Layout.fillHeight: true
            Layout.preferredWidth: 160
            sections: ["Replays", "Statistics", "Settings", "About"]
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: navRail.currentIndex

            ReplaysPage {}
            StatisticsPage {}
            SettingsPage {}
            AboutPage {}
        }
    }
}
