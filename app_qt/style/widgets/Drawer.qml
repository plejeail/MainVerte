import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Templates as T

Basic.Drawer {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space4
    background: Surface { color: Tokens.elevated; border.color: Tokens.border }

    T.Overlay.modal: Rectangle { color: Tokens.scrim }
    T.Overlay.modeless: Rectangle { color: "transparent" }

    topPadding: Tokens.space4 + SafeArea.margins.top
    bottomPadding: Tokens.space4 + SafeArea.margins.bottom
    leftPadding: Tokens.space4 + SafeArea.margins.left
    rightPadding: Tokens.space4 + SafeArea.margins.right
    enter: Transition { NumberAnimation { property: "position"; duration: Tokens.motionDuration } }
    exit: Transition { NumberAnimation { property: "position"; duration: Tokens.motionDuration } }

}
