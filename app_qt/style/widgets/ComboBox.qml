pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic as Basic


Basic.ComboBox {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    property bool invalid: false
    readonly property real popupAvailableHeight: Window.height
    padding: Tokens.space2
    spacing: Tokens.space2
    hoverEnabled: true
    background: ControlSurface { control: root; input: true; invalid: root.invalid }
    popup: Popup {
        y: root.height + Tokens.space1
        width: root.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding,
                         root.popupAvailableHeight - topMargin - bottomMargin)
        margins: Tokens.space2
        padding: Tokens.space1
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.delegateModel
            currentIndex: root.highlightedIndex
            highlightMoveDuration: 0
            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
    delegate: ItemDelegate {
        required property var model
        required property int index
        width: ListView.view.width
        text: model[root.textRole]
        highlighted: root.highlightedIndex === index
        font.weight: root.currentIndex === index ? Font.DemiBold : Font.Normal
    }

}
