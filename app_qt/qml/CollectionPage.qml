pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MainVerteStyle as MV

Page {
    id: page
    title: Ts.get(TextId.Collection_Title)
    PlantModel { id: plants; objectName: "plantModel" }
    property string openedId: ""
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
    Component.onCompleted: plants.refresh()
    Connections {
        target: plants
        function onSaved() { editor.close(); collectionEditor.close(); confirm.close(); grid.positionViewAtBeginning() }
    }
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
                model: plants.collections
                textRole: "name"
                valueRole: "id"
                background: Rectangle { color: MV.Tokens.selection; radius: MV.Tokens.radiusLarge; border.width: selector.visualFocus ? MV.Tokens.focusWidth : 0; border.color: MV.Tokens.focus }
                enabled: !plants.busy
                currentIndex: indexOfValue(plants.selected)
                onActivated: plants.submit(PlantModel.SelectCollection, currentValue)
                Accessible.name: Ts.get(TextId.Collection_Title)
            }
            ToolButton {
                text: "⋮"
                enabled: !plants.busy
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
            enabled: !plants.busy
            onClicked: { page.operation = PlantModel.CreateCollection; collectionName.text = ""; collectionEditor.open() }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: MV.Tokens.space1
            Label {
                objectName: "activeCount"
                text: Ts.get(TextId.Plants_Active).replace("%1", plants.activeCount)
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
                checked: plants.archived
                enabled: !plants.busy
                onToggled: plants.showArchived(checked)
            }
        }
        Label { Layout.fillWidth: true; visible: plants.error.length > 0; text: plants.error; color: MV.Tokens.error; wrapMode: Text.Wrap }
        Button { visible: plants.error.length > 0; text: Ts.get(TextId.Plants_Retry); enabled: !plants.busy; onClicked: plants.refresh() }
        RowLayout {
            visible: plants.busy
            BusyIndicator { running: plants.busy }
            Label { text: Ts.get(TextId.Plants_Loading) }
        }
        GridView {
            id: grid
            objectName: "specimenGrid"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: plants
            enabled: !plants.busy
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
                    page.openedId = card.plantId
                    page.operation = PlantModel.UpdateSpecimen
                    page.originalName = card.plantName
                    page.originalBotanical = card.botanical
                    page.originalCultivar = card.cultivar
                    page.restoreFields()
                    destination.currentIndex = destination.indexOfValue(plants.selected)
                    editor.open()
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
                visible: grid.count === 0 && !plants.busy && plants.error.length === 0
                text: Ts.get(selector.currentIndex < 0 ? TextId.Plants_NoCollection : plants.archived ? TextId.Plants_NoArchived : TextId.Plants_Empty)
            }
        }
        Button {
            Layout.alignment: Qt.AlignRight
            highlighted: true
            objectName: "addPlant"
            text: "+  " + Ts.get(TextId.Plants_Add)
            enabled: !plants.busy && selector.currentIndex >= 0
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
            TextField { enabled: !plants.busy; id: collectionName; Accessible.name: Ts.get(TextId.Plants_Name); Layout.fillWidth: true }
            Label { text: Ts.get(TextId.Plants_Loading); visible: plants.busy }
            Label { text: plants.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Button { text: Ts.get(TextId.Plants_Save); enabled: !plants.busy && collectionName.text.trim().length > 0; onClicked: plants.submit(page.operation, plants.selected, collectionName.text.trim()) }
            Button { text: Ts.get(TextId.Plants_Back); enabled: !plants.busy; onClicked: collectionEditor.close() }
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
            Label { text: Ts.get(TextId.Plants_Loading); visible: plants.busy }
            Label { text: plants.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Button { text: Ts.get(TextId.Plants_Delete); enabled: !plants.busy; onClicked: plants.submit(PlantModel.DeleteCollection, plants.selected) }
            Button { text: Ts.get(TextId.Plants_Back); enabled: !plants.busy; onClicked: confirm.close() }
        }
    }
    Dialog {
        id: editor
        objectName: "specimenDetails"
        x: (page.width - width) / 2
        y: Math.max(12, (page.editorAvailableHeight - height) / 2)
        width: Math.min(page.width - 24, 500)
        height: Math.min(page.editorAvailableHeight - 24, 600)
        closePolicy: plants.busy || page.dirty ? Popup.NoAutoClose : Popup.CloseOnEscape
        modal: true
        title: Ts.get(page.operation === PlantModel.CreateSpecimen ? TextId.Plants_Add : TextId.Plants_Details)
        ScrollView {
            id: editorScroll
            objectName: "detailsScroll"
            anchors.fill: parent
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                Label { visible: page.operation !== PlantModel.CreateSpecimen; text: selector.currentText + " · #" + page.openedId; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: Ts.get(TextId.Plants_Name) }
                TextField { enabled: !plants.busy; id: nameField; onActiveFocusChanged: page.reveal(this); objectName: "specimenName"; Accessible.name: Ts.get(TextId.Plants_Name); Layout.fillWidth: true }
                Label { text: Ts.get(TextId.Plants_Botanical) }
                TextField { enabled: !plants.busy; id: botanicalName; onActiveFocusChanged: page.reveal(this); objectName: "botanicalName"; Accessible.name: Ts.get(TextId.Plants_Botanical); Layout.fillWidth: true }
                Label { text: Ts.get(TextId.Plants_Cultivar) }
                TextField { enabled: !plants.busy; id: cultivarName; objectName: "cultivarName"; onActiveFocusChanged: page.reveal(this); Accessible.name: Ts.get(TextId.Plants_Cultivar); Layout.fillWidth: true }
                Label { text: Ts.get(TextId.Plants_Loading); visible: plants.busy }
                Label { text: plants.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Button {
                    objectName: "specimenSave"
                    highlighted: true
                    text: Ts.get(TextId.Plants_Save)
                    enabled: !plants.busy && nameField.text.trim().length > 0
                    onClicked: plants.submit(page.operation, page.operation === PlantModel.CreateSpecimen ? plants.selected : page.openedId, nameField.text.trim(), botanicalName.text.trim(), cultivarName.text.trim())
                }
                ToolSeparator { visible: page.operation !== PlantModel.CreateSpecimen; orientation: Qt.Horizontal; Layout.fillWidth: true }
                Label { visible: page.operation !== PlantModel.CreateSpecimen; text: Ts.get(TextId.Plants_ManagePlant); font: MV.Tokens.labelFont }
                Label { visible: page.operation !== PlantModel.CreateSpecimen && page.dirty; text: Ts.get(TextId.Plants_SaveBeforeMove); Layout.fillWidth: true; wrapMode: Text.Wrap }
                Button { objectName: "discardEdits"; visible: page.operation !== PlantModel.CreateSpecimen && page.dirty; text: Ts.get(TextId.Plants_Discard); enabled: !plants.busy; onClicked: page.restoreFields() }
                Label { visible: page.operation !== PlantModel.CreateSpecimen; text: Ts.get(TextId.Plants_CurrentCollection) + " " + selector.currentText; Layout.fillWidth: true; wrapMode: Text.Wrap }
                ComboBox { id: destination; objectName: "moveDestination"; Accessible.name: Ts.get(TextId.Plants_Move); visible: page.operation !== PlantModel.CreateSpecimen; Layout.fillWidth: true; enabled: !plants.busy && !page.dirty; model: plants.collections; textRole: "name"; valueRole: "id" }
                Button { objectName: "moveSpecimen"; visible: page.operation !== PlantModel.CreateSpecimen; text: Ts.get(TextId.Plants_Move); enabled: !plants.busy && !page.dirty && destination.currentIndex >= 0 && destination.currentValue !== plants.selected; onClicked: plants.submit(PlantModel.MoveSpecimen, page.openedId, "", "", "", destination.currentValue) }
                Button { visible: page.operation !== PlantModel.CreateSpecimen; text: Ts.get(plants.archived ? TextId.Plants_Restore : TextId.Plants_Archive); enabled: !plants.busy && !page.dirty; onClicked: plants.submit(plants.archived ? PlantModel.RestoreSpecimen : PlantModel.ArchiveSpecimen, page.openedId) }
                Button { text: Ts.get(TextId.Plants_Back); enabled: !plants.busy && (page.operation === PlantModel.CreateSpecimen || !page.dirty); onClicked: editor.close() }
            }
        }
    }
}
