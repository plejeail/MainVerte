import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.RadioButton {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space2
    spacing: Tokens.space3
    hoverEnabled: true
    implicitHeight: Math.max(Tokens.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    indicator: SelectionIndicator { control: root; trailing: false; radio: true }


}
