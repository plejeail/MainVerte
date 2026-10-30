pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Templates as T

Basic.SplitView {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    handle: Rectangle {
        implicitWidth: Tokens.space3
        implicitHeight: Tokens.space3
        color: T.SplitHandle.pressed ? Tokens.pressed : T.SplitHandle.hovered ? Tokens.hover : Tokens.surfaceSecondary
        Rectangle {
            anchors.centerIn: parent
            width: root.orientation === Qt.Horizontal ? Tokens.hairline : parent.width
            height: root.orientation === Qt.Horizontal ? parent.height : Tokens.hairline
            color: Tokens.border
        }
    }

}
