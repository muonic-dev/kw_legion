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

    readonly property color analysisShade: Qt.rgba(0, 0, 0, Theme.lightMode ? 0.06 : 0.25)

    // Fixed-order categorical palette (assign by stable identity - e.g.
    // player index - never by rank, and never cycled/regenerated). Validated
    // via the dataviz skill's validate_palette.js against this app's actual
    // analysis-panel surface (Theme.analysisShade over reallyLight/
    // reallyDark, ~#d9d9d9 light / ~#171717 dark): CVD-safe and
    // normal-vision-safe in both modes. In light mode, 5 of the 8 steps sit
    // below 3:1 contrast against that surface - the accompanying series
    // needs a visible direct label, not color alone.
    readonly property var categoricalSeries: lightMode ? [
        "#2a78d6", "#eb6834", "#1baf7a", "#eda100",
        "#e87ba4", "#008300", "#4a3aa7", "#e34948"
    ] : [
        "#3987e5", "#d95926", "#199e70", "#c98500",
        "#d55181", "#008300", "#9085e9", "#e66767"
    ]

    readonly property url appIcon: AppInfo.debugBuild ? "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Black_Hand_Logo.png" : "qrc:/qt/qml/KWLegionUI/ico/CNCKW_Marked_of_Kane_Logo.png"

    readonly property int shortAnimationDuration: 120
    readonly property int mediumAnimationDuration: 360
}
