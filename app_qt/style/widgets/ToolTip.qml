import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.ToolTip {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space4
    background: Surface { color: Tokens.elevated; border.color: Tokens.border }

}
