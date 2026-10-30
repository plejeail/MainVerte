import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Templates as T

Basic.Dialog {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space4
    background: Surface { color: Tokens.elevated; border.color: Tokens.border }

    T.Overlay.modal: Rectangle { color: Tokens.scrim }
    T.Overlay.modeless: Rectangle { color: "transparent" }

    header: Label {
        text: root.title
        visible: text.length > 0
        font: Tokens.titleFont
        padding: Tokens.space4
        wrapMode: Text.Wrap
    }
    footer: DialogButtonBox { visible: count > 0 }

}
