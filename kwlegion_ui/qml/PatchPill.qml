// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Controls.Basic
import KWLegionUI

Rectangle {
    id: pill

    property alias text: patchLabel.text

    opacity: text.length == 0 ? 0 : 1

    radius: height / 2
    color: "transparent"
    border.color: Theme.lightMode ? Theme.dark : Theme.light
    implicitWidth: patchLabel.implicitWidth + 12
    implicitHeight: patchLabel.implicitHeight + 4

    Label {
        id: patchLabel
        anchors.centerIn: parent
        padding: 2
        font.italic: true
        font.pointSize: Application.font.pointSize - 2
        color: Theme.lightMode ? Theme.dark : Theme.light
    }
}
