// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.platform
import "pages"

ApplicationWindow {
    id: appWindow
    width: 640
    height: 480
    minimumWidth: 200
    minimumHeight: 250
    visible: true
    title: qsTr("LEGION Replay Manager")

    readonly property url appIcon: "qrc:/qt/qml/kw_legion/qml/CNCKW_Marked_of_Kane_logo.png"

    property bool lightMode: Application.styleHints.colorScheme === Qt.Light
    property color reallyDark: "#1f1f1f"
    property color dark: "#262626"
    property color reallyLight: "#e7e7e7"
    property color light: "#e0e0e0"

    SystemTrayIcon {
        visible: true
        icon.source: appWindow.appIcon
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

            ReplaysPage {
                window: appWindow
            }
            StatisticsPage {}
            SettingsPage {}
            AboutPage {}
        }
    }
}
