// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    property var sections: []
    property int currentIndex: 0

    readonly property color navTextColor: window.lightMode ? window.reallyDark : window.reallyLight
    readonly property color navCheckedOverlay: window.lightMode ? Qt.rgba(0, 0, 0, 0.15) : Qt.rgba(1, 1, 1, 0.15)

    color: window.lightMode ? window.light : window.dark

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        Repeater {
            model: root.sections

            delegate: Button {
                id: navButton
                required property string modelData
                required property int index
                Layout.fillWidth: true
                text: modelData
                checkable: true
                checked: root.currentIndex === index
                onClicked: root.currentIndex = index

                contentItem: Text {
                    text: navButton.text
                    color: root.navTextColor
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 4
                    color: navButton.checked ? root.navCheckedOverlay : "transparent"
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
