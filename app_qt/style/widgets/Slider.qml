import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.Slider {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space2
    implicitHeight: horizontal ? Tokens.controlHeight : Tokens.controlHeight * 5
    implicitWidth: horizontal ? Tokens.controlHeight * 5 : Tokens.controlHeight
    handle: SliderHandle { control: root; node: root }
    background: SliderTrack { control: root }

}
