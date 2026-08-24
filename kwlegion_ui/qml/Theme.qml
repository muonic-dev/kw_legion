// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma Singleton

import QtQuick

QtObject {
    readonly property bool lightMode: Application.styleHints.colorScheme === Qt.Light

    readonly property color reallyDark: "#1f1f1f"
    readonly property color dark: "#262626"
    readonly property color reallyLight: "#e7e7e7"
    readonly property color light: "#e0e0e0"

    readonly property url appIcon: "qrc:/qt/qml/KWLegionUI/qml/CNCKW_Marked_of_Kane_Logo.png"
}
