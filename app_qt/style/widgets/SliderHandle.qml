import QtQuick
Rectangle {
    required property var control
    required property var node
    implicitWidth: Tokens.handleSize
    implicitHeight: Tokens.handleSize
    x: control.leftPadding + (control.horizontal ? node.visualPosition * (control.availableWidth - width) : (control.availableWidth - width) / 2)
    y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : node.visualPosition * (control.availableHeight - height))
    radius: width / 2
    color: !control.enabled ? Tokens.disabledSurface : node.pressed ? Tokens.selection : Tokens.elevated
    border.width: Tokens.focusWidth
    border.color: control.enabled ? Tokens.accent : Tokens.disabledText
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.space1
        radius: width / 2
        color: "transparent"
        border.width: Tokens.focusWidth
        border.color: Tokens.focus
        visible: parent.control.visualFocus || parent.activeFocus
    }
}
