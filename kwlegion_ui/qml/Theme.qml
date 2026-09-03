// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma Singleton

import QtQuick
import KWLegionCore

QtObject {
    readonly property bool lightMode: Application.styleHints.colorScheme === Qt.Light

    readonly property color reallyDark: "#1f1f1f"
    readonly property color dark: "#262626"
    readonly property color reallyLight: "#e7e7e7"
    readonly property color light: "#e0e0e0"

    readonly property color selectionTint: Qt.rgba((Theme.lightMode ? Theme.dark : Theme.light).r, (Theme.lightMode ? Theme.dark : Theme.light).g, (Theme.lightMode ? Theme.dark : Theme.light).b, Theme.lightMode ? 0.08 : 0.14)

    readonly property url appIcon: AppInfo.debugBuild ? "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Black_Hand_Logo.png" : "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Marked_of_Kane_Logo.png"

    readonly property int shortAnimationDuration: 120
}
