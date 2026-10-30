import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.Button {
    id: root
    palette: Tokens.palette
    font: Tokens.labelFont

    padding: Tokens.space2
    horizontalPadding: Tokens.space4
    spacing: Tokens.space2
    hoverEnabled: true
    icon.width: Tokens.iconSize
    icon.height: Tokens.iconSize
    background: ControlSurface {
        control: root
        primary: root.highlighted
        selected: root.checked
        quiet: root.flat


    }

}
