// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

StackLayout {
    id: textField

    required property string text

    property bool editing: false
    currentIndex: editing ? 1 : 0

    signal fieldChanged(string text)

    RowLayout {
        id: rowLayout

        visible: !textField.editing
        implicitHeight: edit.implicitHeight

        Label {
            id: displayLabel
            Layout.fillWidth: true
            Layout.fillHeight: true
            verticalAlignment: Text.AlignVCenter
            text: textField.text
            elide: Text.ElideRight
            font.bold: true
            color: Theme.lightMode ? Theme.dark : Theme.light
        }

        Button {
            flat: true
            padding: 4
            Layout.alignment: Qt.AlignVCenter

            contentItem: TintedIcon {
                source: "qrc:/qt/qml/KWLegionUI/ico/edit-pencil-line-01-svgrepo-com.svg"
                sourceSize: Qt.size(12, 12)
            }

            implicitWidth: implicitContentWidth + leftPadding + rightPadding
            implicitHeight: implicitContentHeight + topPadding + bottomPadding

            onClicked: {
                textField.editing = true;
                edit.forceActiveFocus();
                edit.text = textField.text;
                edit.selectAll();
            }
        }
    }

    function commitEdit() {
        // Hiding ourselves blurred
        if (!textField.editing) {
            return;
        }
        textField.editing = false;
        textField.fieldChanged(edit.text);
    }

    TextField {
        id: edit
        visible: textField.editing
        color: Theme.lightMode ? Theme.dark : Theme.light
        selectionColor: Theme.lightMode ? Theme.dark : Theme.light
        selectedTextColor: Theme.lightMode ? Theme.reallyLight : Theme.reallyDark
        onEditingFinished: textField.commitEdit()
        // TODO: Contemplate where to remove focus if the user clicks onto somewhere like the list view
        // It seems that changing pages removes focus so we only need to deal with the current page
        onFocusChanged: if (!focus) {
            textField.commitEdit();
        }

        Keys.onEscapePressed: {
            textField.editing = false;
            edit.text = textField.text;
        }
    }
}
