import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.GroupBox {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space4
    topPadding: padding + (implicitLabelHeight > 0 ? implicitLabelHeight + spacing : 0)
    spacing: Tokens.space2
    label: Text {
        x: root.leftPadding
        y: root.padding
        width: root.availableWidth
        text: root.title
        font: Tokens.labelFont
        color: root.enabled ? Tokens.text : Tokens.disabledText
        elide: Text.ElideRight
    }
    background: Surface { }

}
