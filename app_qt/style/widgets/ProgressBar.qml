import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.ProgressBar {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: 0
    background: Rectangle {
        implicitWidth: Tokens.controlHeight * 5
        implicitHeight: Tokens.space2
        radius: Tokens.radiusSmall
        color: Tokens.separator
    }
    contentItem: Item {
        implicitWidth: Tokens.controlHeight * 5
        implicitHeight: Tokens.space2
        clip: true
        Rectangle {
            property real pulseOpacity: 1
            opacity: root.indeterminate && root.visible ? pulseOpacity : 1
            width: root.indeterminate ? parent.width / 3 : root.position * parent.width
            height: parent.height
            radius: Tokens.radiusSmall
            color: root.enabled ? Tokens.accent : Tokens.disabledText
            x: root.indeterminate ? (parent.width - width) / 2 : root.mirrored ? parent.width - width : 0
            SequentialAnimation on pulseOpacity {
                running: root.indeterminate && root.visible
                loops: Animation.Infinite
                NumberAnimation { from: 0.4; to: 1; duration: 650 }
                NumberAnimation { from: 1; to: 0.4; duration: 650 }
            }
        }
    }

}
