import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    property var sections: []
    property int currentIndex: 0

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

                background: Rectangle {
                    radius: 4
                    color: navButton.checked ? Qt.rgba(0, 0, 0, 0.15) : "transparent"
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
