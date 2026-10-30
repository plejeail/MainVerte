import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.ItemDelegate {
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
        selected: root.highlighted
    }

}
