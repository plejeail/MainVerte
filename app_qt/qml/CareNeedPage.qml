pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import MainVerteStyle as MV

Page {
    id: page
    // These page actions wrap at large font sizes while retaining Button input and accessibility.
    component PageAction: Button {
        id: action
        contentItem: Label {
            text: action.text
            font: action.font
            color: !action.enabled ? MV.Tokens.disabledText : action.highlighted ? MV.Tokens.textOnAccent : MV.Tokens.text
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }
    required property CareModel care
    required property var need
    signal closeRequested()
    title: need.name ?? ""
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton { text: "‹"; Accessible.name: Ts.get(TextId.Plants_Back); enabled: !page.care.busy; onClicked: page.closeRequested() }
            Label { text: page.title; font: MV.Tokens.titleFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
    }
    Connections {
        target: page.care
        function onSaved() { confirmation.close(); page.closeRequested() }
    }
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: MV.Tokens.space5
            Item { Layout.preferredHeight: MV.Tokens.space2 }
            IconImage { source: page.need.icon ?? ""; color: MV.Tokens.accent; sourceSize.width: 64; sourceSize.height: 64; Layout.alignment: Qt.AlignHCenter }
            Label { text: page.title; font: MV.Tokens.headingFont; Layout.fillWidth: true; Layout.margins: MV.Tokens.space4; wrapMode: Text.Wrap }
            Label { text: page.need.description ?? ""; color: MV.Tokens.textSecondary; Layout.fillWidth: true; Layout.leftMargin: MV.Tokens.space4; Layout.rightMargin: MV.Tokens.space4; wrapMode: Text.Wrap }
            Frame {
                Layout.fillWidth: true
                Layout.margins: MV.Tokens.space4
                ColumnLayout {
                    width: parent.width
                    spacing: MV.Tokens.space3
                    Label { text: Ts.get(TextId.Care_NotConfigured); font: MV.Tokens.labelFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Label { text: Ts.get(TextId.Care_ScheduleInfo); color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                }
            }
            PageAction { objectName: "removeNeed"; text: Ts.get(TextId.Care_Remove); Layout.fillWidth: true; Layout.margins: MV.Tokens.space4; enabled: !page.care.busy; onClicked: confirmation.open() }
        }
    }
    Dialog {
        id: confirmation
        objectName: "removeNeedConfirmation"
        anchors.centerIn: parent
        width: Math.min(page.width - MV.Tokens.space6, 440)
        height: Math.min(implicitHeight, page.height - MV.Tokens.space6)
        modal: true
        closePolicy: page.care.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
        title: Ts.get(TextId.Care_Remove)
        ScrollView {
            anchors.fill: parent
            contentWidth: availableWidth
            implicitHeight: confirmationContent.implicitHeight
            ColumnLayout {
                id: confirmationContent
                width: parent.width
                Label { text: Ts.get(TextId.Care_RemoveConfirm); Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: page.care.error; color: MV.Tokens.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: Ts.get(TextId.Plants_Loading); visible: page.care.busy; Layout.fillWidth: true; wrapMode: Text.Wrap }
                PageAction { objectName: "confirmRemoveNeed"; text: Ts.get(TextId.Care_Remove); enabled: !page.care.busy; Layout.fillWidth: true; onClicked: page.care.remove(page.need.id) }
                PageAction { text: Ts.get(TextId.Plants_Back); enabled: !page.care.busy; Layout.fillWidth: true; onClicked: confirmation.close() }
            }
        }
    }
}
