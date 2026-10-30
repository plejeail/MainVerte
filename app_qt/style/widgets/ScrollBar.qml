import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.ScrollBar {
    id: root
    palette: Tokens.palette


    padding: Tokens.space1
    contentItem: Rectangle {
        implicitWidth: Tokens.space2
        implicitHeight: Tokens.space2
        radius: Tokens.radiusSmall
        color: root.enabled ? Tokens.border : Tokens.disabledText
        opacity: root.policy === Basic.ScrollBar.AlwaysOn || root.active || root.hovered || root.pressed ? 1 : 0
    }

}
