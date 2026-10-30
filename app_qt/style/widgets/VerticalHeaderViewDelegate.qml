import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.VerticalHeaderViewDelegate {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space2
    implicitHeight: Math.max(Tokens.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    background: ControlSurface {
        control: root
        selected: root.highlighted
        color: root.highlighted ? Tokens.selection : Tokens.surfaceSecondary
        focused: root.visualFocus || root.current === true
    }

}
