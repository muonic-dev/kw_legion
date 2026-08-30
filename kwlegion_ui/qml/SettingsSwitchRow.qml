// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import KWLegionUI

ColumnLayout {
    property alias name: nameLabel.text
    property alias description: descriptionLabel.text
    property alias checked: toggle.checked

    Layout.alignment: Qt.AlignHCenter
    Layout.maximumWidth: 640

    spacing: 2

    RowLayout {
        Column {
            Label {
                id: nameLabel
                Layout.fillWidth: true
                font.bold: true
                color: Theme.lightMode ? Theme.dark : Theme.light
            }
            Label {
                id: descriptionLabel
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: Theme.lightMode ? Theme.dark : Theme.light
                opacity: 0.7
            }
        }
        Item {
            Layout.fillWidth: true
        }
        Switch {
            id: toggle
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
