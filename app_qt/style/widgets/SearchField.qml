pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic as Basic


Basic.SearchField {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont
    readonly property real popupAvailableHeight: Window.height

    padding: Tokens.space2
    background: ControlSurface { control: root; input: true }
    searchIndicator.indicator: Item {
        implicitWidth: Tokens.controlHeight
        implicitHeight: Tokens.controlHeight
        x: root.mirrored ? root.width - width : 0
        y: (root.height - height) / 2
        Rectangle {
            anchors.centerIn: parent
            width: Tokens.space3
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Tokens.focusWidth
            border.color: root.enabled ? Tokens.textSecondary : Tokens.disabledText
            Rectangle { x: parent.width - 1; y: parent.height - 1; width: Tokens.space2; height: Tokens.focusWidth; rotation: 45; color: parent.border.color }
        }
    }
    clearIndicator.indicator: Item {
        implicitWidth: Tokens.controlHeight
        implicitHeight: Tokens.controlHeight
        x: root.mirrored ? 0 : root.width - width
        y: (root.height - height) / 2
        visible: root.text.length > 0
        Rectangle { anchors.centerIn: parent; width: Tokens.space3; height: Tokens.focusWidth; rotation: 45; color: root.enabled ? Tokens.textSecondary : Tokens.disabledText }
        Rectangle { anchors.centerIn: parent; width: Tokens.space3; height: Tokens.focusWidth; rotation: -45; color: root.enabled ? Tokens.textSecondary : Tokens.disabledText }
    }
    delegate: ItemDelegate {
        required property var model
        required property int index
        width: ListView.view.width
        text: model[root.textRole]
        highlighted: root.highlightedIndex === index
    }
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

}
