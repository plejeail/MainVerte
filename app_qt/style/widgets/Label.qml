import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.Label {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    color: enabled ? Tokens.text : Tokens.disabledText
    linkColor: Tokens.accent
    lineHeight: Tokens.lineHeight

}
