import QtQuick

Rectangle {
    required property var control
    property bool primary: false
    property bool selected: false
    property bool quiet: false
    property bool invalid: false
    property bool input: false
    readonly property bool isDown: control.down === true || control.pressed === true
    property bool focused: control.visualFocus || (input && control.activeFocus)
    implicitWidth: Tokens.controlHeight * 2.5
    implicitHeight: Tokens.controlHeight
    radius: Tokens.radius
    color: !control.enabled ? Tokens.disabledSurface
        : primary || selected ? (isDown ? Tokens.accentPressed : control.hovered ? Tokens.accentHover : Tokens.accent)
        : isDown ? Tokens.pressed : control.hovered ? Tokens.hover
        : quiet ? "transparent" : input ? Tokens.elevated : Tokens.surfaceSecondary
    border.width: quiet && !Tokens.highContrast ? 0 : Tokens.hairline
    border.color: invalid ? Tokens.error : primary || selected ? color : Tokens.border

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.space1
        radius: parent.radius + Tokens.space1
        color: "transparent"
        border.width: Tokens.focusWidth
        border.color: parent.invalid ? Tokens.error : Tokens.focus
        visible: parent.focused
    }
}
