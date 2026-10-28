import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    property int currentSection: 0
    readonly property bool desktopNavigation: Qt.platform.os !== "android"

    function showSection(index: int) {
        if (currentSection === index)
            return;
        currentSection = index;
        pages.replace(index === 0 ? collectionPage : settingsPage);
    }

    height: 480
    title: Ts.get(TextId.App_Title)
    visible: true
    width: 640

    footer: TabBar {
        currentIndex: window.currentSection
        visible: !window.desktopNavigation

        TabButton {
            text: Ts.get(TextId.Collection_Title)

            onClicked: window.showSection(0)
        }

        TabButton {
            text: Ts.get(TextId.Parameters_Title)

            onClicked: window.showSection(1)
        }
    }
    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                font.bold: true
                text: (pages.currentItem as Page)?.title ?? Ts.get(TextId.App_Title)
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Frame {
            Layout.fillHeight: true
            Layout.preferredWidth: 180
            visible: window.desktopNavigation

            ColumnLayout {
                anchors.fill: parent

                Button {
                    Layout.fillWidth: true
                    autoExclusive: true
                    checkable: true
                    checked: window.currentSection === 0
                    text: Ts.get(TextId.Collection_Title)

                    onClicked: window.showSection(0)
                }

                Button {
                    Layout.fillWidth: true
                    autoExclusive: true
                    checkable: true
                    checked: window.currentSection === 1
                    text: Ts.get(TextId.Parameters_Title)

                    onClicked: window.showSection(1)
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        StackView {
            id: pages

            Layout.fillHeight: true
            Layout.fillWidth: true
            initialItem: collectionPage
        }
    }

    Component {
        id: collectionPage

        CollectionPage {
        }
    }

    Component {
        id: settingsPage

        SettingsPage {
        }
    }
}
