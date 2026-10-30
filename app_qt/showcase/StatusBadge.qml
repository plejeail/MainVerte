import QtQuick
import QtQuick.Controls
import MainVerteStyle as MV

Label {
    property color statusColor: MV.Tokens.success
    color: statusColor
    font: MV.Tokens.captionFont
    padding: MV.Tokens.space2
    background: Rectangle {
        radius: MV.Tokens.radiusSmall
        color: MV.Tokens.surfaceSecondary
        border.width: MV.Tokens.hairline
        border.color: MV.Tokens.separator
    }
}
