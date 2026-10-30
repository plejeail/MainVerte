import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.RadioDelegate {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space2
    spacing: Tokens.space3
    hoverEnabled: true
    implicitHeight: Math.max(Tokens.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    indicator: SelectionIndicator { control: root; trailing: true; radio: true }
    background: ControlSurface { control: root; quiet: true; color: root.highlighted ? Tokens.selection : root.down ? Tokens.pressed : root.hovered ? Tokens.hover : "transparent" }

}
