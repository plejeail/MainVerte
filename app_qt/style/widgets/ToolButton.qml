import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.ToolButton {
    id: root
    palette: Tokens.palette
    font: Tokens.labelFont

    padding: Tokens.space2
    horizontalPadding: Tokens.space2
    spacing: Tokens.space2
    hoverEnabled: true
    icon.width: Tokens.iconSize
    icon.height: Tokens.iconSize
    contentItem: ActionContent { control: root; selected: root.highlighted || root.checked; centered: true }
    background: ControlSurface {
        control: root
        primary: root.highlighted
        selected: root.checked
        quiet: root.flat
        implicitWidth: Tokens.controlHeight

    }

}
