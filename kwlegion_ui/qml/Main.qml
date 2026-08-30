// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.platform
import KWLegionUI
import KWLegionCore
import "pages"

ApplicationWindow {
    id: appWindow
    width: 1100
    height: 700
    minimumWidth: 200
    minimumHeight: 250
    visible: false // so we can control via the on completed

    title: qsTr("LEGION Replay Manager")

    Component.onCompleted: {
        console.log("switch", AppInfo.startMinimized, "setting", Settings.startMinimized);
        const beVisible = !AppInfo.startMinimized && !Settings.startMinimized;
        appWindow.visible = beVisible;
    }

    property bool quitting: false

    onClosing: close => {
        if (quitting) {
            return;
        }
        close.accepted = false;
        appWindow.hide();
    }

    Shortcut {
        sequence: "CTRL+Q" // TODO: Crossplatform
        onActivated: {
            appWindow.quitting = true;
            Qt.quit();
        }
    }

    SystemTrayIcon {
        visible: true
        icon.source: Theme.appIcon
        tooltip: appWindow.title

        onActivated: reason => {
            if (reason === SystemTrayIcon.Trigger || reason === SystemTrayIcon.DoubleClick) {
                if (appWindow.visible) {
                    appWindow.hide();
                } else {
                    appWindow.show();
                    appWindow.raise();
                    appWindow.requestActivate();
                }
            }
        }

        menu: Menu {
            MenuItem {
                text: qsTr("Show")
                onTriggered: {
                    appWindow.show();
                    appWindow.raise();
                    appWindow.requestActivate();
                }
            }
            MenuItem {
                text: qsTr("Quit")
                onTriggered: {
                    appWindow.quitting = true;
                    Qt.quit();
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavRail {
            id: navRail
            Layout.fillHeight: true
            Layout.preferredWidth: 160
            sections: ["Replays",
                // "Statistics",
                "Settings", "About"]
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: navRail.currentIndex

            ReplaysPage {}
            // StatisticsPage {}
            SettingsPage {}
            AboutPage {}
        }
    }
}
