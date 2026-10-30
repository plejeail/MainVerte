import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.TabButton {
    id: root
    palette: Tokens.palette
    font: Tokens.labelFont

    padding: Tokens.space2
    horizontalPadding: Tokens.space3
    spacing: Tokens.space2
    hoverEnabled: true
    icon.width: Tokens.iconSize
    icon.height: Tokens.iconSize
    contentItem: ActionContent { control: root; selected: root.checked; centered: true }
    background: ControlSurface {
        control: root
        quiet: true
        selected: root.checked
    }

}
