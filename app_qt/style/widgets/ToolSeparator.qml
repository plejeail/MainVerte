import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.ToolSeparator {
    id: root
    palette: Tokens.palette


    padding: Tokens.space1
    contentItem: Rectangle {
        implicitWidth: root.vertical ? Tokens.hairline : Tokens.controlHeight
        implicitHeight: root.vertical ? Tokens.controlHeight : Tokens.hairline
        color: Tokens.separator
    }

}
