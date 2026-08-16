// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
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
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.lightMode ? Theme.dark : Theme.light
            text: qsTr(
                "Copyright © 2026 Muonic\n\n" +
                "This program is free software: you can redistribute it and/or " +
                "modify it under the terms of the GNU General Public License as " +
                "published by the Free Software Foundation, either version 3 of " +
                "the License, or (at your option) any later version.\n\n" +
                "This program is distributed in the hope that it will be useful, " +
                "but WITHOUT ANY WARRANTY; without even the implied warranty of " +
                "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU " +
                "General Public License for more details.")
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.italic: true
            color: Theme.lightMode ? Theme.dark : Theme.light
            text: qsTr(
                "This application is built with Qt 6, © The Qt Company Ltd. " +
                "and other contributors, used here under the GNU Lesser General " +
                "Public License version 3 (LGPLv3); Qt is linked dynamically so " +
                "its shared libraries may be replaced or relinked as the LGPL " +
                "permits. See https://www.qt.io/licensing/ for details.")
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
