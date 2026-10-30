import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.TreeViewDelegate {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space2
    implicitHeight: Math.max(Tokens.controlHeight, implicitContentHeight + Tokens.space2 * 2)
    background: ControlSurface {
        control: root
        selected: root.highlighted
        focused: root.visualFocus || root.current
    }

}
