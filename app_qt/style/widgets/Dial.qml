import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.Dial {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    background: Rectangle {
        implicitWidth: Tokens.controlHeight * 3
        implicitHeight: implicitWidth
        radius: width / 2
        color: Tokens.surfaceSecondary
        border.width: Tokens.focusWidth
        border.color: root.visualFocus ? Tokens.focus : root.enabled ? Tokens.border : Tokens.disabledText
    }
    handle: Rectangle {
        x: root.background.x + root.background.width / 2 - width / 2
        y: root.background.y + root.background.height / 2 - height / 2
        implicitWidth: Tokens.space2
        implicitHeight: Tokens.space4
        radius: Tokens.radiusSmall
        color: root.enabled ? Tokens.accent : Tokens.disabledText
        transform: [
            Translate { y: -Math.min(root.background.width, root.background.height) * 0.34 },
            Rotation { angle: root.angle; origin.x: root.handle.width / 2; origin.y: root.handle.height / 2 }
        ]
    }

}
