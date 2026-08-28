// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import KWLegionCore
import KWLegionUI

Page {
    background: Rectangle {
        color: Theme.lightMode ? Theme.reallyLight : Theme.reallyDark
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            source: Theme.appIcon
            fillMode: Image.PreserveAspectFit
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("LEGION Replay Manager")
            color: Theme.lightMode ? Theme.dark : Theme.light
            font.pixelSize: 24
            font.bold: true
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Version %1").arg(AppInfo.version)
            color: Theme.lightMode ? Theme.dark : Theme.light
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Build %1").arg(AppInfo.buildHash)
            color: Theme.lightMode ? Theme.dark : Theme.light
            font.pixelSize: 12
        }

        Label {
            Layout.fillWidth: true
            Layout.maximumWidth: 480
            Layout.alignment: Qt.AlignHCenter
            wrapMode: Text.WordWrap
            color: Theme.lightMode ? Theme.dark : Theme.light
            textFormat: Text.StyledText
            linkColor: Theme.lightMode ? Theme.dark : Theme.light
            onLinkActivated: link => Qt.openUrlExternally(link)
            text: qsTr(`Copyright © 2026 Muonic

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the <a href="https://www.gnu.org/licenses/gpl-3.0.en.html#license-text">GNU General Public License version 3</a> for more details.`)
        }

        Label {
            Layout.fillWidth: true
            Layout.maximumWidth: 480
            Layout.alignment: Qt.AlignHCenter
            wrapMode: Text.WordWrap
            font.italic: true
            color: Theme.lightMode ? Theme.dark : Theme.light
            textFormat: Text.StyledText
            linkColor: Theme.lightMode ? Theme.dark : Theme.light
            onLinkActivated: link => Qt.openUrlExternally(link)
            text: qsTr(`This application is built with Qt 6, © The Qt Company Ltd. and other contributors, used here under the <a href="https://www.gnu.org/licenses/lgpl-3.0.en.html#license-text">GNU Lesser General Public License version 3 (LGPLv3)</a>`)
            HoverHandler {
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.maximumWidth: 480
            Layout.alignment: Qt.AlignHCenter
            wrapMode: Text.WordWrap
            color: Theme.lightMode ? Theme.dark : Theme.light
            textFormat: Text.StyledText
            linkColor: Theme.lightMode ? Theme.dark : Theme.light
            onLinkActivated: link => Qt.openUrlExternally(link)
            text: qsTr(`Vectors and icons by <a href="https://www.svgrepo.com">SVG Repo</a>`)
            HoverHandler {
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.maximumWidth: 480
            Layout.alignment: Qt.AlignHCenter
            wrapMode: Text.WordWrap
            color: Theme.lightMode ? Theme.dark : Theme.light
            textFormat: Text.StyledText
            linkColor: Theme.lightMode ? Theme.dark : Theme.light
            onLinkActivated: link => Qt.openUrlExternally(link)
            text: qsTr(`Found a bug? Please <a href="https://github.com/muonic-dev/kw_legion">report it on GitHub</a>, and consider attaching the log file from your <a href="%1">local log folder</a>.`).arg(AppInfo.logDirectoryUrl)
            HoverHandler {
                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
