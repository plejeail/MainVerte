pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.BusyIndicator {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space1
    contentItem: Item {
        implicitWidth: Tokens.controlHeight
        implicitHeight: Tokens.controlHeight
        opacity: root.running ? 1 : 0
        Repeater {
            model: 8
            Rectangle {
                id: petal
                required property int index
                x: (parent.width - width) / 2
                y: Tokens.space1
                width: Tokens.space1
                height: Tokens.space2
                radius: Tokens.space1 / 2
                color: root.enabled ? Tokens.accent : Tokens.disabledText
                opacity: (index + 1) / 8
                transform: Rotation { origin.x: Tokens.space1 / 2; origin.y: (root.availableHeight - Tokens.space2) / 2; angle: petal.index * 45 }
            }
        }
        RotationAnimator on rotation {
            running: root.running && root.visible
            from: 0; to: 360; duration: 1000; loops: Animation.Infinite
        }
    }

}
