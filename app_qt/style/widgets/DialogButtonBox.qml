import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.DialogButtonBox {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space4
    spacing: Tokens.space2
    delegate: Button {
        highlighted: DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
            || DialogButtonBox.buttonRole === DialogButtonBox.YesRole
        flat: !highlighted
    }
    background: Rectangle { color: "transparent" }

}
