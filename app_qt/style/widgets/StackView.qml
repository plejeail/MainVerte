import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.StackView {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    pushEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Tokens.motionDuration } }
    pushExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Tokens.motionDuration } }
    popEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Tokens.motionDuration } }
    popExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Tokens.motionDuration } }
    replaceEnter: pushEnter
    replaceExit: pushExit

}
