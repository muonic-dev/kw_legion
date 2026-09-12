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

    // Snackbar hint: reminds the user that autostart-on-login is available.
    // Declared after the RowLayout above so it paints on top of the page content.
    Rectangle {
        id: autostartSnackbar
        color: Theme.lightMode ? Theme.light : Theme.dark
        border.width: 1
        border.color: Theme.lightMode ? Theme.dark : Theme.light
        width: content.implicitWidth + 32
        height: content.implicitHeight + 12
        x: (parent.width - width) / 2
        y: Settings.hasDismissedAutostart ? parent.height : parent.height - (height + 16)
        Behavior on y {
            MediumAnimation {
                id: slideAnimation
            }
        }

        RowLayout {
            id: content
            anchors.centerIn: parent
            SettingDescription {
                name: qsTr("Start automatically")
                description: qsTr("Start at login. LEGION needs to be running in order to track your replays.")
                Layout.fillWidth: true
                rightPadding: 24
            }

            Button {
                contentItem: TintedIcon {
                    source: "qrc:/qt/qml/KWLegionUI/ico/check-svgrepo-com.svg"
                    sourceSize: Qt.size(16, 16)
                }
                onClicked: {
                    Settings.shouldAutostart = true;
                }
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignVCenter
                padding: 10
                implicitWidth: 16 + leftPadding + rightPadding
                implicitHeight: 16 + topPadding + bottomPadding
            }
            Button {
                contentItem: TintedIcon {
                    source: "qrc:/qt/qml/KWLegionUI/ico/close-md-svgrepo-com.svg"
                    sourceSize: Qt.size(16, 16)
                }
                onClicked: {
                    Settings.shouldAutostart = false;
                }
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignVCenter

                padding: 10
                implicitWidth: 16 + leftPadding + rightPadding
                implicitHeight: 16 + topPadding + bottomPadding
            }
        }
    }
}
