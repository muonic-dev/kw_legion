// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import KWLegionUI
import KWLegionCore

Page {
    id: page

    background: Rectangle {
        color: Theme.lightMode ? Theme.reallyLight : Theme.reallyDark
    }

    // Translucent red, theme-adaptive rather than a flat hex, same trick as
    // Theme.selectionTint - keeps the terminal-row tint legible in both
    // light and dark mode instead of just reading as "light red" literally.
    readonly property color terminalTint: Qt.rgba(1, 0, 0, Theme.lightMode ? 0.10 : 0.16)

    ListView {
        id: inboxListView
        model: IngestionModel
        anchors.fill: parent
        anchors.margins: 16
        clip: true
        spacing: 8

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 6
                radius: width / 2
                color: Theme.lightMode ? Theme.dark : Theme.light
            }
        }

        delegate: Rectangle {
            id: delegateRoot

            required property string path
            required property int type
            required property var observedAt

            readonly property bool terminal: type !== InboxType.PENDING

            width: ListView.view.width - 6 // some slight padding for the scrollbar
            height: rowLayout.implicitHeight + 16
            radius: 4
            color: delegateRoot.terminal ? page.terminalTint : "transparent"

            RowLayout {
                id: rowLayout
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                // Reserved even on terminal rows so the path label's left
                // edge lines up with pending rows - the red background
                // already carries the "problem" signal there, so this slot
                // just goes empty rather than duplicating the dismiss
                // button's close glyph.
                Item {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20

                    TintedIcon {
                        id: pendingIcon
                        anchors.fill: parent
                        visible: !delegateRoot.terminal
                        source: "qrc:/qt/qml/KWLegionUI/ico/arrow-reload-02-svgrepo-com.svg"
                        sourceSize: Qt.size(20, 20)

                        // Qt's SVG renderer only rasterizes a static frame -
                        // it doesn't execute SMIL animation - so "spinning"
                        // is a plain rotation of the still icon, not an
                        // animated asset. Same glyph doubles as the future
                        // manual-retry action icon.
                        RotationAnimation on rotation {
                            running: pendingIcon.visible
                            loops: Animation.Infinite
                            from: 0
                            to: 360
                            duration: 1200
                        }
                    }
                }

                Column {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        width: parent.width
                        text: delegateRoot.path
                        elide: Text.ElideMiddle
                        color: Theme.lightMode ? Theme.dark : Theme.light
                    }

                    Label {
                        width: parent.width
                        text: delegateRoot.observedAt.toLocaleString(Qt.locale(), Locale.ShortFormat)
                        font.pixelSize: 11
                        opacity: 0.7
                        elide: Text.ElideRight
                        color: Theme.lightMode ? Theme.dark : Theme.light
                    }
                }

                // Fixed-width slot so terminal and pending rows keep the
                // same text column width - Row/positioner layouts collapse
                // invisible children's space, which would otherwise shift
                // the path label's right edge between row states.
                Item {
                    Layout.preferredWidth: dismissButton.implicitWidth
                    Layout.preferredHeight: dismissButton.implicitHeight

                    Button {
                        id: dismissButton
                        visible: delegateRoot.terminal
                        flat: true

                        contentItem: TintedIcon {
                            source: "qrc:/qt/qml/KWLegionUI/ico/close-sm-svgrepo-com.svg"
                            sourceSize: Qt.size(14, 14)
                        }

                        onClicked: IngestionModel.acknowledgeItem(delegateRoot.path)

                        padding: 8
                        implicitWidth: implicitContentWidth + leftPadding + rightPadding
                        implicitHeight: implicitContentHeight + topPadding + bottomPadding
                    }
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: inboxListView.count === 0
        text: qsTr("Inbox is empty")
        opacity: 0.6
        color: Theme.lightMode ? Theme.dark : Theme.light
        font.pixelSize: 20
    }
}
