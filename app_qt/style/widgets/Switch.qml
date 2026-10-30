import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.Switch {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space2
    spacing: Tokens.space3
    hoverEnabled: true
    implicitHeight: Math.max(Tokens.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    indicator: SwitchIndicator { control: root; trailing: false;  }


}
