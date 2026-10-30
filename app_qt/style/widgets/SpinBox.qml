import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.SpinBox {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    property bool invalid: false
    padding: Tokens.space1
    background: ControlSurface { control: root; input: true; invalid: root.invalid }
    up.indicator: ControlSurface {
        control: root
        focused: false
        implicitWidth: Tokens.controlHeight
        implicitHeight: Tokens.controlHeight
        height: root.height
        x: root.mirrored ? 0 : root.width - width
        color: !root.up.indicator.enabled ? Tokens.disabledSurface : root.up.pressed ? Tokens.pressed : root.up.hovered ? Tokens.hover : Tokens.surfaceSecondary
        Text { anchors.centerIn: parent; text: "+"; font: Tokens.titleFont; color: root.up.indicator.enabled ? Tokens.text : Tokens.disabledText }
    }
    down.indicator: ControlSurface {
        control: root
        focused: false
        implicitWidth: Tokens.controlHeight
        implicitHeight: Tokens.controlHeight
        height: root.height
        x: root.mirrored ? root.width - width : 0
        color: !root.down.indicator.enabled ? Tokens.disabledSurface : root.down.pressed ? Tokens.pressed : root.down.hovered ? Tokens.hover : Tokens.surfaceSecondary
        Text { anchors.centerIn: parent; text: "−"; font: Tokens.titleFont; color: root.down.indicator.enabled ? Tokens.text : Tokens.disabledText }
    }

}
