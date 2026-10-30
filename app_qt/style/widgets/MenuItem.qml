import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.MenuItem {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space2
    horizontalPadding: Tokens.space3
    spacing: Tokens.space2
    hoverEnabled: true
    icon.width: Tokens.iconSize
    icon.height: Tokens.iconSize
    background: ControlSurface {
        control: root
        quiet: true
        selected: false
        color: !root.enabled ? Tokens.disabledSurface : root.down ? Tokens.pressed
            : root.highlighted || root.hovered ? Tokens.hover : "transparent"
    }

}
