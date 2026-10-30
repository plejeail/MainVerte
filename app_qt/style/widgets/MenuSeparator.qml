import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.MenuSeparator {
    id: root
    palette: Tokens.palette


    padding: Tokens.space1
    contentItem: Rectangle {
        implicitWidth: Tokens.controlHeight
        implicitHeight: Tokens.hairline
        color: Tokens.separator
    }

}
