pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MainVerteStyle as MV

Page {
    id: page
    title: Ts.get(TextId.Collection_Title)
    PlantModel { id: plantStore; objectName: "plantModel" }
    readonly property bool showingDetail: navigation.depth > 1
    function requestLeave(proceed: var) {
        const detail = navigation.currentItem as SpecimenDetailPage
        if (detail) detail.requestLeave(proceed)
        else proceed()
    }
    property int operation: PlantModel.CreateSpecimen
    property string originalName: ""
    property string originalBotanical: ""
    property string originalCultivar: ""
    readonly property real editorAvailableHeight: {
        const keyboard = Qt.inputMethod.keyboardRectangle
        if (!Qt.inputMethod.visible) return page.height
        if (keyboard.height > 0) return Math.max(120, Math.min(page.height, keyboard.y - page.mapToItem(null, 0, 0).y))
        return page.height * 0.65
    }
    readonly property bool dirty: nameField.text !== originalName || botanicalName.text !== originalBotanical || cultivarName.text !== originalCultivar
    function restoreFields() {
        nameField.text = originalName
        botanicalName.text = originalBotanical
        cultivarName.text = originalCultivar
    }
    function reveal(field: Item) {
        if (field.activeFocus) Qt.callLater(function() {
            const flick = editorScroll.contentItem as Flickable
            if (flick) flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, field.y - flick.height / 3))
        })
    }
    Component.onCompleted: plantStore.refresh()
    Connections {
        target: plantStore
        function onSaved() { editor.close(); collectionEditor.close(); confirm.close(); grid.positionViewAtBeginning() }
    }
    StackView { id: navigation; anchors.fill: parent; initialItem: gridPage }
    Component {
        id: detailComponent
        SpecimenDetailPage {
            plants: plantStore
            onCloseRequested: { navigation.pop(); grid.forceActiveFocus() }
        }
    }
    Item {
        id: gridPage
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: MV.Tokens.space4
            spacing: MV.Tokens.space2
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: selector
                    objectName: "collectionSelector"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    model: plantStore.collections
                    textRole: "name"
                    valueRole: "id"
                    background: Rectangle { color: MV.Tokens.selection; radius: MV.Tokens.radiusLarge; border.width: selector.visualFocus ? MV.Tokens.focusWidth : 0; border.color: MV.Tokens.focus }
                    enabled: !plantStore.busy
                    currentIndex: indexOfValue(plantStore.selected)
                    onActivated: plantStore.submit(PlantModel.SelectCollection, currentValue)
                    Accessible.name: Ts.get(TextId.Collection_Title)
                }
                ToolButton {
                    text: "⋮"
                    enabled: !plantStore.busy
                    Accessible.name: Ts.get(TextId.Plants_Manage)
                    onClicked: collectionMenu.open()
                    Menu {
                        id: collectionMenu
                        MenuItem { text: Ts.get(TextId.Plants_NewCollection); onTriggered: { page.operation = PlantModel.CreateCollection; collectionName.text = ""; collectionEditor.open() } }
                        MenuItem { text: Ts.get(TextId.Plants_Rename); enabled: selector.currentIndex >= 0; onTriggered: { page.operation = PlantModel.RenameCollection; collectionName.text = selector.currentText; collectionEditor.open() } }
                        MenuItem { text: Ts.get(TextId.Plants_Delete); enabled: selector.currentIndex >= 0; onTriggered: confirm.open() }
                    }
                }
            }
            Button {
                visible: selector.currentIndex < 0
                text: Ts.get(TextId.Plants_NewCollection)
                enabled: !plantStore.busy
                onClicked: { page.operation = PlantModel.CreateCollection; collectionName.text = ""; collectionEditor.open() }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: MV.Tokens.space1
                Label {
                    objectName: "activeCount"
                    text: Ts.get(TextId.Plants_Active).replace("%1", plantStore.activeCount)
                    font: MV.Tokens.captionFont
                    color: MV.Tokens.textSecondary
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    elide: Text.ElideRight
                }
                CheckBox {
                    objectName: "archivedToggle"
                    text: Ts.get(TextId.Plants_FilterArchived)
                    font: MV.Tokens.captionFont
                    checked: plantStore.archived
                    enabled: !plantStore.busy
                    onToggled: plantStore.showArchived(checked)
                }
            }
            Label { Layout.fillWidth: true; visible: plantStore.error.length > 0; text: plantStore.error; color: MV.Tokens.error; wrapMode: Text.Wrap }
            Button { visible: plantStore.error.length > 0; text: Ts.get(TextId.Plants_Retry); enabled: !plantStore.busy; onClicked: plantStore.refresh() }
            RowLayout {
                visible: plantStore.busy
                BusyIndicator { running: plantStore.busy }
                Label { text: Ts.get(TextId.Plants_Loading) }
            }
            GridView {
                id: grid
                objectName: "specimenGrid"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: plantStore
                enabled: !plantStore.busy
                focus: true
                keyNavigationEnabled: true
                highlight: null
                readonly property int columns: page.width < MV.Tokens.compactBreakpoint ? 2 : Math.max(2, Math.floor(width / 220))
                Keys.onReturnPressed: (currentItem as SpecimenCard)?.openDetails()
                Keys.onEnterPressed: (currentItem as SpecimenCard)?.openDetails()
                cellWidth: width / columns
                cellHeight: Math.max(cellWidth, Math.ceil((nameMetrics.height + 1) * MV.Tokens.headingLineHeight * 2) + 64)
                FontMetrics { id: nameMetrics; font: MV.Tokens.labelFont }
                ScrollBar.vertical: ScrollBar {}
                delegate: SpecimenCard { }
                component SpecimenCard: Item {
                    id: card
                    required property int index
                    function openDetails() {
                        grid.currentIndex = card.index
                        navigation.push(detailComponent, { specimenId: card.plantId })
                    }
                    required property string plantId
                    required property string plantName
                    required property string botanical
                    required property string cultivar
                    width: grid.cellWidth
                    height: grid.cellHeight
                    Button {
                        id: surface
                        objectName: "specimenCard"
                        anchors.fill: parent
                        anchors.margins: MV.Tokens.space1
                        padding: MV.Tokens.space3
                        Accessible.name: card.plantName
                        onClicked: card.openDetails()
                        background: Rectangle {
                            radius: MV.Tokens.radiusLarge
                            color: surface.down ? MV.Tokens.pressed : surface.hovered ? MV.Tokens.hover : MV.Tokens.surfaceSecondary
                            border.width: surface.visualFocus || (card.GridView.isCurrentItem && grid.activeFocus) ? MV.Tokens.focusWidth : 0
                            border.color: MV.Tokens.focus
                        }
                        contentItem: ColumnLayout {
                            spacing: MV.Tokens.space1
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumHeight: 28
                                Item {
                                    anchors.centerIn: parent
                                    width: 56
                                    height: 56
                                    scale: Math.min(1.4, parent.height / 56)
                                    Rectangle { width: 2; height: 42; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; color: MV.Tokens.accent }
                                    Rectangle { width: 30; height: 18; radius: 14; x: 0; y: 14; rotation: 35; color: MV.Tokens.accent; opacity: 0.65 }
                                    Rectangle { width: 30; height: 18; radius: 14; x: 26; y: 2; rotation: -35; color: MV.Tokens.accent }
                                }
                            }
                            Label {
                                objectName: "cardName"
                                text: card.plantName
                                font: MV.Tokens.labelFont
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.ceil((nameMetrics.height + 1) * MV.Tokens.headingLineHeight * 2)
                                lineHeight: MV.Tokens.headingLineHeight
                                verticalAlignment: Text.AlignBottom
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    visible: grid.count === 0 && !plantStore.busy && plantStore.error.length === 0
                    text: Ts.get(selector.currentIndex < 0 ? TextId.Plants_NoCollection : plantStore.archived ? TextId.Plants_NoArchived : TextId.Plants_Empty)
                }
            }
            Button {
                Layout.alignment: Qt.AlignRight
                highlighted: true
                objectName: "addPlant"
                text: "+  " + Ts.get(TextId.Plants_Add)
                enabled: !plantStore.busy && selector.currentIndex >= 0
                onClicked: { page.operation = PlantModel.CreateSpecimen; page.originalName = ""; page.originalBotanical = ""; page.originalCultivar = ""; page.restoreFields(); editor.open() }
            }
        }
        Dialog {
            id: collectionEditor
            anchors.centerIn: parent
            width: Math.min(page.width - 24, 440)
            modal: true
            title: Ts.get(page.operation === PlantModel.CreateCollection ? TextId.Plants_NewCollection : TextId.Plants_Rename)
            ColumnLayout {
                width: parent.width
                Label { text: Ts.get(TextId.Plants_Name) }
                TextField { enabled: !plantStore.busy; id: collectionName; Accessible.name: Ts.get(TextId.Plants_Name); Layout.fillWidth: true }
                Label { text: Ts.get(TextId.Plants_Loading); visible: plantStore.busy }
                Label { text: plantStore.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Button { text: Ts.get(TextId.Plants_Save); enabled: !plantStore.busy && collectionName.text.trim().length > 0; onClicked: plantStore.submit(page.operation, plantStore.selected, collectionName.text.trim()) }
                Button { text: Ts.get(TextId.Plants_Back); enabled: !plantStore.busy; onClicked: collectionEditor.close() }
            }
        }
        Dialog {
            id: confirm
            anchors.centerIn: parent
            width: Math.min(page.width - 24, 440)
            modal: true
            title: Ts.get(TextId.Plants_Delete)
            ColumnLayout {
                width: parent.width
                Label { text: Ts.get(TextId.Plants_DeleteConfirm); Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: Ts.get(TextId.Plants_Loading); visible: plantStore.busy }
                Label { text: plantStore.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Button { text: Ts.get(TextId.Plants_Delete); enabled: !plantStore.busy; onClicked: plantStore.submit(PlantModel.DeleteCollection, plantStore.selected) }
                Button { text: Ts.get(TextId.Plants_Back); enabled: !plantStore.busy; onClicked: confirm.close() }
            }
        }
        Dialog {
            id: editor
            objectName: "specimenCreator"
            x: (page.width - width) / 2
            y: Math.max(12, (page.editorAvailableHeight - height) / 2)
            width: Math.min(page.width - 24, 500)
            height: Math.min(page.editorAvailableHeight - 24, 600)
            closePolicy: plantStore.busy || page.dirty ? Popup.NoAutoClose : Popup.CloseOnEscape
            modal: true
            title: Ts.get(TextId.Plants_Add)
            ScrollView {
                id: editorScroll
                objectName: "creationScroll"
                anchors.fill: parent
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    Label { text: Ts.get(TextId.Plants_Name) }
                    TextField { enabled: !plantStore.busy; id: nameField; onActiveFocusChanged: page.reveal(this); objectName: "newSpecimenName"; Accessible.name: Ts.get(TextId.Plants_Name); Layout.fillWidth: true }
                    Label { text: Ts.get(TextId.Plants_Botanical) }
                    TextField { enabled: !plantStore.busy; id: botanicalName; onActiveFocusChanged: page.reveal(this); objectName: "newBotanicalName"; Accessible.name: Ts.get(TextId.Plants_Botanical); Layout.fillWidth: true }
                    Label { text: Ts.get(TextId.Plants_Cultivar) }
                    TextField { enabled: !plantStore.busy; id: cultivarName; objectName: "newCultivarName"; onActiveFocusChanged: page.reveal(this); Accessible.name: Ts.get(TextId.Plants_Cultivar); Layout.fillWidth: true }
                    Label { text: Ts.get(TextId.Plants_Loading); visible: plantStore.busy }
                    Label { text: plantStore.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Button {
                        objectName: "createSpecimenSave"
                        highlighted: true
                        text: Ts.get(TextId.Plants_Save)
                        enabled: !plantStore.busy && nameField.text.trim().length > 0
                        onClicked: plantStore.submit(PlantModel.CreateSpecimen, plantStore.selected, nameField.text.trim(), botanicalName.text.trim(), cultivarName.text.trim())
                    }
                    Button { text: Ts.get(TextId.Plants_Back); enabled: !plantStore.busy && (page.operation === PlantModel.CreateSpecimen || !page.dirty); onClicked: editor.close() }
                }
            }
        }
    }
}
