// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import KWLegionUI

Rectangle {
    id: root

    required property string title
    property string description: ""
    property bool active: true

    property string confirmIcon: "qrc:/qt/qml/KWLegionUI/ico/check-svgrepo-com.svg"
    property string dismissIcon: "qrc:/qt/qml/KWLegionUI/ico/close-md-svgrepo-com.svg"

    signal confirmed
    signal dismissed

    color: Theme.lightMode ? Theme.light : Theme.dark
    border.width: 1
    border.color: Theme.lightMode ? Theme.dark : Theme.light
    implicitWidth: content.implicitWidth + 32
    implicitHeight: content.implicitHeight + 12

    RowLayout {
        id: content
        anchors.centerIn: parent

        SettingDescription {
            name: root.title
            description: root.description
            Layout.fillWidth: true
            rightPadding: 24
        }

        Button {
            contentItem: TintedIcon {
                source: root.confirmIcon
                sourceSize: Qt.size(16, 16)
            }
            onClicked: root.confirmed()
            padding: 10
            implicitWidth: 16 + leftPadding + rightPadding
            implicitHeight: 16 + topPadding + bottomPadding
        }

        Button {
            contentItem: TintedIcon {
                source: root.dismissIcon
                sourceSize: Qt.size(16, 16)
            }
            onClicked: root.dismissed()
            padding: 10
            implicitWidth: 16 + leftPadding + rightPadding
            implicitHeight: 16 + topPadding + bottomPadding
        }
    }
}
