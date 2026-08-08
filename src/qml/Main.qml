import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.platform
import "pages"

ApplicationWindow {
    id: window
    width: 640
    height: 480
    minimumWidth: 200
    minimumHeight: 250
    visible: true
    title: qsTr("LEGION Replay Manager")
    property bool lightMode: Application.styleHints.colorScheme === Qt.Light
    property color reallyDark: "#1f1f1f"
    property color dark: "#262626"
    property color reallyLight: "#e7e7e7"
    property color light: "#e0e0e0"

    SystemTrayIcon {
        visible: true
        icon.source: "qrc:/qt/qml/kw_legion/qml/CNCKW_Marked_of_Kane_logo.png"
        tooltip: window.title
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavRail {
            id: navRail
            Layout.fillHeight: true
            Layout.preferredWidth: 160
            sections: ["Replays", "Statistics", "Settings", "About"]
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: navRail.currentIndex

            ReplaysPage {}
            StatisticsPage {}
            SettingsPage {}
            AboutPage {}
        }
    }
}
