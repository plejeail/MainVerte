import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.SwitchDelegate {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space2
    spacing: Tokens.space3
    hoverEnabled: true
    implicitHeight: Math.max(Tokens.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    indicator: SwitchIndicator { control: root; trailing: true;  }
    background: ControlSurface { control: root; quiet: true; color: root.highlighted ? Tokens.selection : root.down ? Tokens.pressed : root.hovered ? Tokens.hover : "transparent" }

}
