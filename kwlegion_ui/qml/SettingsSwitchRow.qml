// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import KWLegionUI

ColumnLayout {
    id: control
    property alias name: settingDesc.name
    property alias description: settingDesc.description
    property alias checked: toggle.checked

    Layout.alignment: Qt.AlignHCenter
    Layout.maximumWidth: 640

    spacing: 2

    RowLayout {
        SettingDescription {
            id: settingDesc
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
