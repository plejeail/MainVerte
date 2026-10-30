import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.RangeSlider {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    padding: Tokens.space2
    implicitHeight: horizontal ? Tokens.controlHeight : Tokens.controlHeight * 5
    implicitWidth: horizontal ? Tokens.controlHeight * 5 : Tokens.controlHeight
    first.handle: SliderHandle { control: root; node: root.first }
    second.handle: SliderHandle { control: root; node: root.second }
    background: SliderTrack { control: root; start: root.first.position; end: root.second.position }

}
