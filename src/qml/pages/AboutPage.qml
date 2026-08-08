import QtQuick
import QtQuick.Controls.Basic

Page {
    background: Rectangle {
        color: window.lightMode ? window.reallyLight : window.reallyDark
    }
    Label {
        anchors.centerIn: parent
        text: qsTr("About")
        color: window.lightMode ? window.dark : window.light
        font.pixelSize: 32
    }
}
