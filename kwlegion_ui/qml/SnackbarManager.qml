// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

import QtQuick
import KWLegionUI

Item {
    id: manager
    anchors.fill: parent
    z: 10

    default property list<SnackbarSpec> declarativeSnackbars

    ListView {
        id: listView
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        width: parent.width
        height: contentHeight
        spacing: 8
        model: manager.declarativeSnackbars

        delegate: Item {
            id: delegateRoot
            required property SnackbarSpec modelData

            width: ListView.view.width
            height: modelData.active ? snackbar.implicitHeight : 0
            clip: true

            Behavior on height {
                ShortAnimation {}
            }

            Snackbar {
                id: snackbar

                anchors.horizontalCenter: parent.horizontalCenter
                title: delegateRoot.modelData.title
                description: delegateRoot.modelData.description

                visible: delegateRoot.modelData.active

                onConfirmed: delegateRoot.modelData.confirmed()
                onDismissed: delegateRoot.modelData.dismissed()
            }
        }

        displaced: Transition {
            ShortYAnimator {
                easing.type: Easing.OutQuad
            }
        }
    }
}
