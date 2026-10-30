import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.SelectionRectangle {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    topLeftHandle: SelectionHandle { }
    bottomRightHandle: SelectionHandle { }

    component SelectionHandle: Item {
        width: Tokens.targetSize
        height: Tokens.targetSize
        visible: Basic.SelectionRectangle.control.active
        Rectangle {
            anchors.centerIn: parent
            width: Tokens.indicatorSize
            height: width
            radius: width / 2
            color: Basic.SelectionRectangle.dragging ? Tokens.selection : Tokens.elevated
            border.color: Tokens.accent
            border.width: Tokens.focusWidth
        }
    }

}
