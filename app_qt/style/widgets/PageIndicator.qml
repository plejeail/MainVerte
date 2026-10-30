// Qt 6.12 supplies delegate indices through a mutable context; keep this unbound.
import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.PageIndicator {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    spacing: Tokens.space2
    padding: Tokens.space2
    delegate: Rectangle {
        id: dot
        required property int index
        implicitWidth: Tokens.targetSize
        implicitHeight: Tokens.targetSize
        color: "transparent"
        Rectangle {
            anchors.centerIn: parent
            width: dot.index === root.currentIndex ? Tokens.space5 : Tokens.space2
            height: Tokens.space2
            radius: Tokens.radiusSmall
            color: root.enabled ? Tokens.accent : Tokens.disabledText
        }
    }
    background: ControlSurface { control: root; quiet: true; implicitWidth: 0; implicitHeight: 0 }

}
