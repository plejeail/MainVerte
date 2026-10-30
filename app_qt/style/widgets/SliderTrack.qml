import QtQuick
Rectangle {
    required property var control
    property real start: 0
    property real end: control.position
    x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
    y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)
    implicitWidth: control.horizontal ? Tokens.controlHeight * 5 : Tokens.trackSize
    implicitHeight: control.horizontal ? Tokens.trackSize : Tokens.controlHeight * 5
    width: control.horizontal ? control.availableWidth : implicitWidth
    height: control.horizontal ? implicitHeight : control.availableHeight
    radius: Tokens.trackSize / 2
    color: Tokens.border
    scale: control.horizontal && control.mirrored ? -1 : 1
    Rectangle {
        x: parent.control.horizontal ? parent.start * parent.width : 0
        y: parent.control.horizontal ? 0 : (1 - parent.end) * parent.height
        width: parent.control.horizontal ? (parent.end - parent.start) * parent.width : parent.width
        height: parent.control.horizontal ? parent.height : (parent.end - parent.start) * parent.height
        radius: parent.radius
        color: parent.control.enabled ? Tokens.accent : Tokens.disabledText
    }
}
