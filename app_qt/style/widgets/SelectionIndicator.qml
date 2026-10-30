import QtQuick

Rectangle {
    id: indicator
    required property var control
    property bool radio: false
    property bool trailing: false
    readonly property bool partial: !radio && control.checkState === Qt.PartiallyChecked
    implicitWidth: Tokens.indicatorSize
    implicitHeight: Tokens.indicatorSize
    x: trailing ? (control.mirrored ? control.leftPadding : control.width - width - control.rightPadding)
        : control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding)
        : control.leftPadding + (control.availableWidth - width) / 2
    y: control.topPadding + (control.availableHeight - height) / 2
    radius: radio ? width / 2 : Tokens.radiusSmall
    color: !control.enabled ? Tokens.disabledSurface : control.checked || partial ? Tokens.accent
        : control.down ? Tokens.pressed : control.hovered ? Tokens.hover : Tokens.elevated
    border.color: !control.enabled ? Tokens.disabledText : control.checked || partial ? Tokens.accent : Tokens.border
    border.width: Tokens.hairline

    Rectangle {
        anchors.centerIn: parent
        width: indicator.radio ? Tokens.space2 : Tokens.space3
        height: indicator.radio ? width : Tokens.hairline * 2
        radius: indicator.radio ? width / 2 : 0
        color: indicator.control.enabled ? Tokens.textOnAccent : Tokens.disabledText
        visible: indicator.radio ? indicator.control.checked : indicator.partial
    }
    // Vector check avoids a font-dependent symbol and remains crisp at every DPI.
    Item {
        anchors.centerIn: parent
        width: Tokens.space3
        height: Tokens.space3
        visible: !indicator.radio && indicator.control.checked && !indicator.partial
        Rectangle { x: 1; y: 6; width: 5; height: 2; rotation: 45; color: indicator.control.enabled ? Tokens.textOnAccent : Tokens.disabledText }
        Rectangle { x: 4; y: 4; width: 9; height: 2; rotation: -45; color: indicator.control.enabled ? Tokens.textOnAccent : Tokens.disabledText }
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.space1
        radius: parent.radius + Tokens.space1
        color: "transparent"
        border.color: Tokens.focus
        border.width: Tokens.focusWidth
        visible: indicator.control.visualFocus
    }
}
