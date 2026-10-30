// TumblerView uses a mutable delegate context in Qt 6.12; keep this unbound.
import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.Tumbler {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    background: ControlSurface { control: root; input: true }
    delegate: Text {
        required property var modelData
        required property int index
        text: modelData
        font: Math.abs(Tumbler.displacement) < 0.5 ? Tokens.labelFont : Tokens.bodyFont
        color: !root.enabled ? Tokens.disabledText : Math.abs(Tumbler.displacement) < 0.5 ? Tokens.text : Tokens.textSecondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

}
