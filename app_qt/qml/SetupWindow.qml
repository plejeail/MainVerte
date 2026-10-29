import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window

    title: Ts.get(TextId.App_Title)
    visible: Qt.platform.os === "android"

    Component.onCompleted: {
        if (Qt.platform.os !== "android") {
            Qt.callLater(function () {
                setupPage.prepareLayout();
                window.x = window.Screen.virtualX + (window.Screen.width - window.width) / 2;
                window.y = window.Screen.virtualY + (window.Screen.height - window.height) / 2;
                window.visible = true;
            });
        }
    }

    Binding {
        property: "width"
        target: window
        value: 640
        when: Qt.platform.os !== "android"
    }

    Binding {
        property: "height"
        target: window
        value: Math.min(setupPage.implicitHeight, Screen.desktopAvailableHeight - 80)
        when: Qt.platform.os !== "android"
    }

    Page_wizard_setup {
        id: setupPage

        anchors.fill: parent
    }
}
