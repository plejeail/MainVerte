import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window

    title: Ts.get(TextId.App_Title)
    flags: Qt.platform.os === "android" ? Qt.Window | Qt.ExpandedClientAreaHint : Qt.Window
    color: "#143F2C"
    visible: Qt.platform.os === "android"

    // ApplicationWindow keeps the form inside the safe area; its background
    // extends behind the transparent Android system bars.
    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#143F2C" }
            GradientStop { position: 0.55; color: "#0C3427" }
            GradientStop { position: 1.0; color: "#071F19" }
        }
    }

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
