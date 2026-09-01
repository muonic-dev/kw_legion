// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Controls.Basic
import KWLegionUI

Page {
    background: Rectangle {
        color: Theme.lightMode ? Theme.reallyLight : Theme.reallyDark
    }
    Label {
        anchors.centerIn: parent
        text: qsTr("Inbox")
        color: Theme.lightMode ? Theme.dark : Theme.light
        font.pixelSize: 32
    }
}
