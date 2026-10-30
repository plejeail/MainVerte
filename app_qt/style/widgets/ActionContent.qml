import QtQuick
import QtQuick.Controls.impl

// Qt's icon/text layout preserves display modes, icon sources, mnemonics and RTL.
IconLabel {
    required property var control
    property bool selected: false
    property bool centered: false
    readonly property color ink: !control.enabled ? Tokens.disabledText : selected ? Tokens.textOnAccent : Tokens.text
    spacing: control.spacing
    mirrored: control.mirrored
    display: control.display
    alignment: centered ? Qt.AlignCenter : Qt.AlignLeft
    icon: control.icon
    text: control.text
    font: control.font
    color: ink
    defaultIconColor: ink
}
