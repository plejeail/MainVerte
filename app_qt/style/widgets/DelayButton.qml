import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.DelayButton {
    id: root
    palette: Tokens.palette
    font: Tokens.labelFont

    padding: Tokens.space2
    horizontalPadding: Tokens.space4
    background: ControlSurface {
        control: root
        Rectangle {
            width: root.progress * parent.width
            height: parent.height
            radius: parent.radius
            color: Tokens.accent
        }
    }
    // The Basic content preserves its two clipped labels as the hold progresses.

}
