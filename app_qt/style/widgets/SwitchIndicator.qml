import QtQuick

Rectangle {
    id: indicator
    required property var control
    property bool trailing: false
    implicitWidth: Tokens.indicatorSize * 2
    implicitHeight: Tokens.indicatorSize + Tokens.space1
    x: trailing ? (control.mirrored ? control.leftPadding : control.width - width - control.rightPadding)
        : control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding)
        : control.leftPadding + (control.availableWidth - width) / 2
    y: control.topPadding + (control.availableHeight - height) / 2
    radius: height / 2
    color: !control.enabled ? Tokens.disabledSurface : control.checked ? Tokens.accent : control.down ? Tokens.pressed : Tokens.surfaceSecondary
    border.width: Tokens.hairline
    border.color: control.enabled ? Tokens.border : Tokens.disabledText
    Rectangle {
        x: Tokens.space1 / 2 + indicator.control.visualPosition * (parent.width - width - Tokens.space1)
        y: (parent.height - height) / 2
        width: Tokens.indicatorSize - Tokens.space1
        height: width
        radius: width / 2
        color: !indicator.control.enabled ? Tokens.disabledText : indicator.control.checked ? Tokens.textOnAccent : Tokens.accent
        Behavior on x { enabled: !indicator.control.down; NumberAnimation { duration: Tokens.motionDuration } }
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.space1
        radius: height / 2
        color: "transparent"
        border.color: Tokens.focus
        border.width: Tokens.focusWidth
        visible: indicator.control.visualFocus
    }
}
