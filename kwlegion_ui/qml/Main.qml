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
        const beVisible = !AppInfo.startMinimized && !Settings.startMinimized;
        appWindow.visible = beVisible;
    }

    property bool quitting: false

    onClosing: close => {
        if (quitting) {
            return;
        }
        if (Settings.closeToTray) {
            close.accepted = false;
            appWindow.hide();
        } else {
            appWindow.quitting = true;
            Qt.quit();
        }
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
            sections: [
                QtObject {
                    readonly property string label: "Replays"
                    readonly property bool showable: true
                },
                QtObject {
                    readonly property string label: "Inbox"
                    readonly property bool showable: IngestionModel.ingestionCount > 0
                },
                QtObject {
                    readonly property string label: "Statistics"
                    readonly property bool showable: true
                },
                QtObject {
                    readonly property string label: "Settings"
                    readonly property bool showable: true
                },
                QtObject {
                    readonly property string label: "About"
                    readonly property bool showable: true
                }
            ]
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: navRail.currentIndex

            ReplaysPage {}
            InboxPage {}
            StatisticsPage {}
            SettingsPage {}
            AboutPage {}
        }
    }
}
