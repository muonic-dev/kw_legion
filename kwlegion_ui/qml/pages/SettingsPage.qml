// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Controls.Basic

Page {
    background: Rectangle {
        color: window.lightMode ? window.reallyLight : window.reallyDark
    }
    Label {
        anchors.centerIn: parent
        text: qsTr("Settings")
        color: window.lightMode ? window.dark : window.light
        font.pixelSize: 32
    }
}
