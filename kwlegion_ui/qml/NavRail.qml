// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import KWLegionUI

Rectangle {
    id: root

    property list<QtObject> sections: []
    property int currentIndex: 0

    readonly property color navTextColor: Theme.lightMode ? Theme.reallyDark : Theme.reallyLight
    readonly property color navCheckedOverlay: Theme.lightMode ? Qt.rgba(0, 0, 0, 0.15) : Qt.rgba(1, 1, 1, 0.15)

    color: Theme.lightMode ? Theme.light : Theme.dark

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        Repeater {
            model: root.sections

            delegate: Button {
                id: navButton
                required property string label
                required property bool showable
                required property int index
                Layout.fillWidth: true
                text: label
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
