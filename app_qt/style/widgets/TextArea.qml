import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.TextArea {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    // Explicit application validation; acceptableInput alone also covers intermediate input.
    property bool invalid: false
    color: enabled ? Tokens.text : Tokens.disabledText
    placeholderTextColor: enabled ? Tokens.textTertiary : Tokens.disabledText
    selectionColor: Tokens.accent
    selectedTextColor: Tokens.textOnAccent
    padding: Tokens.space3
    selectByMouse: true
    background: ControlSurface {
        control: root
        input: true
        invalid: root.invalid
        focused: root.activeFocus
        implicitHeight: Tokens.controlHeight * 3
    }

}
