import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    readonly property bool desktopNavigation: Qt.platform.os !== "android"
    property int currentSection: 0

    function showSection(index: int) {
        if (currentSection === index)
            return
        currentSection = index
        pages.replace(index === 0 ? collectionPage : settingsPage)
    }

    width: 640
    height: 480
    visible: true
    title: qsTr("MainVerte")

    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            Label {
                text: (pages.currentItem as Page)?.title ?? ""
                font.bold: true
                Layout.fillWidth: true
                Layout.leftMargin: 12
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Frame {
            visible: window.desktopNavigation
            Layout.preferredWidth: 180
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent

                Button {
                    text: qsTr("Collection")
                    Layout.fillWidth: true
                    checkable: true
                    autoExclusive: true
                    checked: window.currentSection === 0
                    onClicked: window.showSection(0)
                }

                Button {
                    text: qsTr("Paramètres")
                    Layout.fillWidth: true
                    checkable: true
                    autoExclusive: true
                    checked: window.currentSection === 1
                    onClicked: window.showSection(1)
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        StackView {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            initialItem: collectionPage
        }
    }

    footer: TabBar {
        visible: !window.desktopNavigation
        currentIndex: window.currentSection

        TabButton {
            text: qsTr("Collection")
            onClicked: window.showSection(0)
        }

        TabButton {
            text: qsTr("Paramètres")
            onClicked: window.showSection(1)
        }
    }

    Component {
        id: collectionPage
        CollectionPage {}
    }

    Component {
        id: settingsPage
        SettingsPage {}
    }
}
