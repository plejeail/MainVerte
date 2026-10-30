import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Templates as T

Basic.Popup {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space4
    background: Surface { color: Tokens.elevated; border.color: Tokens.border }

    T.Overlay.modal: Rectangle { color: Tokens.scrim }
    T.Overlay.modeless: Rectangle { color: "transparent" }

}
