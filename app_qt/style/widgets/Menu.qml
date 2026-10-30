import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Menu {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    background: Surface { color: Tokens.elevated; border.color: Tokens.border }
    padding: Tokens.space1
    delegate: MenuItem { }

}
