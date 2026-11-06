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
    objectName: "specimenDetails"
    focus: true
    required property PlantModel plants
    required property string specimenId
    property var specimen: plants.specimen(specimenId)
    property bool editing: false
    property var activeNeed: null
    property var discardContinuation: null
    property int submittedAction: -1
    readonly property bool dirty: nameField.text !== (specimen.name ?? "") || botanicalField.text !== (specimen.botanical ?? "") || cultivarField.text !== (specimen.cultivar ?? "") || profileField.currentValue !== specimen.profile || acquiredField.text !== (specimen.acquiredOn ?? "")
    readonly property bool pending: plants.busy || careStore.busy
    readonly property var profiles: [
        { value: -1, name: Ts.get(TextId.Specimen_ProfileUnknown) },
        { value: 0, name: Ts.get(TextId.Profile_Succulent) },
        { value: 1, name: Ts.get(TextId.Profile_Tropical) },
        { value: 2, name: Ts.get(TextId.Profile_FruitTree) },
        { value: 3, name: Ts.get(TextId.Profile_Herb) },
        { value: 4, name: Ts.get(TextId.Profile_Other) }
    ]
    signal closeRequested()
    title: specimen.name ?? Ts.get(TextId.Plants_Details)
    function restoreFields() {
        nameField.text = specimen.name ?? ""
        botanicalField.text = specimen.botanical ?? ""
        cultivarField.text = specimen.cultivar ?? ""
        profileField.currentIndex = profileField.indexOfValue(specimen.profile ?? -1)
        acquiredField.text = specimen.acquiredOn ?? ""
    }
    function close() {
        if (pending) return
        if (activeNeed !== null) { activeNeed = null; return }
        requestLeave(function() { page.closeRequested() })
    }
    function requestLeave(proceed: var) {
        if (pending) return
        if (editing && dirty) {
            discardContinuation = proceed
            discard.open()
        } else proceed()
    }
    function discardEdits() {
        restoreFields()
        editing = false
        plants.discardSpecimenEditError(specimenId)
    }
    function submit(action: int) {
        submittedAction = action
        plants.submit(action, specimenId, nameField.text.trim(), botanicalField.text.trim(), cultivarField.text.trim(), destination.currentValue ?? "", profileField.currentValue, acquiredField.text.trim())
    }
    function reveal(field: Item) {
        if (!field.activeFocus) return
        Qt.callLater(function() {
            if (!field || !field.activeFocus || !scroll || !form) return
            const flick = scroll.contentItem as Flickable
            if (flick) flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, field.mapToItem(form, 0, 0).y + form.y - flick.height / 3))
        })
    }
    Component.onCompleted: restoreFields()
    Keys.onEscapePressed: close()
    Keys.onBackPressed: close()
    CareModel { id: careStore; objectName: "careModel"; specimenId: page.specimenId }
    Connections {
        target: page.plants
        function onSaved() {
            if (page.submittedAction === PlantModel.UpdateSpecimen) {
                page.specimen = page.plants.specimen(page.specimenId)
                page.restoreFields()
                destination.currentIndex = destination.indexOfValue(page.specimen.collectionId)
                page.editing = false
            } else if (page.submittedAction >= 0) page.closeRequested()
            page.submittedAction = -1
        }
    }
    Connections { target: careStore; function onSaved() { typeSelector.close() } }
    header: ToolBar {
        visible: page.activeNeed === null
        RowLayout {
            anchors.fill: parent
            ToolButton { objectName: "detailBack"; text: "‹"; Accessible.name: Ts.get(TextId.Plants_Back); enabled: !page.pending; onClicked: page.close() }
            Label { text: page.title; maximumLineCount: 2; elide: Text.ElideRight; font: MV.Tokens.titleFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
        }
    }
    ScrollView {
        id: scroll
        objectName: "detailsScroll"
        anchors.fill: parent
        anchors.bottomMargin: {
            const keyboard = Qt.inputMethod.keyboardRectangle
            if (!Qt.inputMethod.visible) return 0
            return keyboard.height > 0 ? Math.max(0, page.mapToItem(null, 0, page.height).y - keyboard.y) : page.height * 0.35
        }
        visible: page.activeNeed === null
        contentWidth: availableWidth
        ColumnLayout {
            id: content
            width: Math.min(scroll.availableWidth, 720)
            x: (scroll.availableWidth - width) / 2
            spacing: MV.Tokens.space5
            // Self-contained portrait slot: a future photo can replace its contents.
            Rectangle {
                Layout.fillWidth: true
                Layout.margins: MV.Tokens.space4
                implicitHeight: portrait.implicitHeight + MV.Tokens.space6
                radius: MV.Tokens.radiusLarge
                color: MV.Tokens.surfaceSecondary
                clip: true
                Rectangle { width: 180; height: 180; radius: 90; anchors.right: parent.right; y: -60; color: MV.Tokens.selection }
                ColumnLayout {
                    id: portrait
                    anchors.centerIn: parent
                    width: parent.width - MV.Tokens.space6
                    spacing: MV.Tokens.space3
                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 140
                        implicitHeight: 150
                        Rectangle { x: 69; y: 20; width: 3; height: 88; radius: 2; color: MV.Tokens.accent }
                        Rectangle { x: 18; y: 47; width: 55; height: 30; radius: 24; rotation: 30; color: MV.Tokens.healthy }
                        Rectangle { x: 69; y: 22; width: 53; height: 30; radius: 24; rotation: -32; color: MV.Tokens.accent }
                        Rectangle { x: 70; y: 67; width: 45; height: 25; radius: 22; rotation: -25; color: MV.Tokens.healthy }
                        Rectangle { x: 39; y: 105; width: 65; height: 39; radius: 10; color: MV.Tokens.soil }
                        Rectangle { x: 35; y: 101; width: 73; height: 10; radius: 4; color: MV.Tokens.secondaryAccent }
                    }
                    Label { text: Ts.get(TextId.Specimen_Portrait); font: MV.Tokens.titleFont; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                    Label { text: Ts.get(TextId.Specimen_PhotoPlaceholder); font: MV.Tokens.captionFont; color: MV.Tokens.textSecondary; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MV.Tokens.space4
                Layout.rightMargin: MV.Tokens.space4
                spacing: MV.Tokens.space2
                Label { objectName: "detailDisplayName"; text: page.title; font: MV.Tokens.headingFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: page.specimen.botanical ?? ""; visible: text.length > 0; color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: page.specimen.cultivar ?? ""; visible: text.length > 0; color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: page.profiles[(page.specimen.profile ?? -1) + 1].name; visible: page.specimen.profile >= 0; font: MV.Tokens.labelFont; color: MV.Tokens.healthy; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { objectName: "acquisitionDuration"; text: page.specimen.duration || (page.specimen.acquiredLabel ? Ts.get(TextId.Specimen_Acquired) + " · " + page.specimen.acquiredLabel : Ts.get(TextId.Specimen_DateUnknown)); color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: page.specimen.acquiredLabel ?? ""; visible: (page.specimen.duration ?? "").length > 0; font: MV.Tokens.captionFont; color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { text: Ts.get(TextId.Plants_Archived); visible: page.specimen.archived ?? false; font: MV.Tokens.captionFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                PageAction { objectName: "editSpecimen"; text: Ts.get(TextId.Specimen_Edit); visible: !page.editing; enabled: !page.pending; Layout.fillWidth: true; onClicked: { page.restoreFields(); page.editing = true; nameField.forceActiveFocus() } }
            }
            Frame {
                visible: page.editing
                Layout.fillWidth: true
                Layout.leftMargin: MV.Tokens.space4
                Layout.rightMargin: MV.Tokens.space4
                ColumnLayout {
                    id: form
                    width: parent.width
                    spacing: MV.Tokens.space3
                    Label { text: Ts.get(TextId.Plants_Name); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    TextField { id: nameField; objectName: "specimenName"; Accessible.name: Ts.get(TextId.Plants_Name); Layout.fillWidth: true; enabled: !page.pending; onActiveFocusChanged: page.reveal(this) }
                    Label { text: Ts.get(TextId.Plants_Botanical); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    TextField { id: botanicalField; objectName: "botanicalName"; Accessible.name: Ts.get(TextId.Plants_Botanical); Layout.fillWidth: true; enabled: !page.pending; onActiveFocusChanged: page.reveal(this) }
                    Label { text: Ts.get(TextId.Plants_Cultivar); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    TextField { id: cultivarField; objectName: "cultivarName"; Accessible.name: Ts.get(TextId.Plants_Cultivar); Layout.fillWidth: true; enabled: !page.pending; onActiveFocusChanged: page.reveal(this) }
                    Label { text: Ts.get(TextId.Specimen_Profile); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    ComboBox { id: profileField; objectName: "specimenProfile"; model: page.profiles; textRole: "name"; valueRole: "value"; Accessible.name: Ts.get(TextId.Specimen_Profile); Layout.fillWidth: true; enabled: !page.pending }
                    Label { text: Ts.get(TextId.Specimen_Acquired); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    TextField { id: acquiredField; objectName: "acquiredOn"; Accessible.name: Ts.get(TextId.Specimen_Acquired); Accessible.description: Ts.get(TextId.Specimen_DateHint); placeholderText: "YYYY-MM-DD"; Layout.fillWidth: true; enabled: !page.pending; inputMethodHints: Qt.ImhDate; onActiveFocusChanged: page.reveal(this) }
                    Label { text: Ts.get(TextId.Specimen_DateHint); color: MV.Tokens.textSecondary; font: MV.Tokens.captionFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    PageAction { objectName: "specimenSave"; text: Ts.get(TextId.Plants_Save); highlighted: true; enabled: !page.pending && nameField.text.trim().length > 0; Layout.fillWidth: true; onClicked: page.submit(PlantModel.UpdateSpecimen) }
                    PageAction { objectName: "discardEdits"; text: Ts.get(TextId.Plants_Discard); enabled: !page.pending; Layout.fillWidth: true; onClicked: page.discardEdits() }
                }
            }
            Label { objectName: "specimenError"; text: page.plants.error; visible: text.length > 0; color: MV.Tokens.error; Layout.fillWidth: true; Layout.margins: MV.Tokens.space4; wrapMode: Text.Wrap }
            Label { text: Ts.get(TextId.Plants_Loading); visible: page.plants.busy; Layout.fillWidth: true; Layout.margins: MV.Tokens.space4; wrapMode: Text.Wrap }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MV.Tokens.space4
                Layout.rightMargin: MV.Tokens.space4
                spacing: MV.Tokens.space3
                Label { text: Ts.get(TextId.Care_Title); font: MV.Tokens.titleFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { objectName: "careEmpty"; text: Ts.get(TextId.Care_Empty); visible: careStore.loaded && careStore.needs.length === 0; color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Repeater {
                    model: careStore.needs
                    delegate: ItemDelegate {
                        id: needRow
                        required property var modelData
                        objectName: "need_" + modelData.type
                        Layout.fillWidth: true
                        enabled: !page.pending && !page.dirty
                        Accessible.name: modelData.name
                        Accessible.description: Ts.get(TextId.Care_NotConfigured)
                        text: modelData.name
                        contentItem: RowLayout {
                            spacing: MV.Tokens.space3
                            IconImage { source: needRow.modelData.icon; color: MV.Tokens.accent; sourceSize.width: 28; sourceSize.height: 28 }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Label { text: needRow.modelData.name; font: MV.Tokens.labelFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                                Label { text: Ts.get(TextId.Care_NotConfigured); color: MV.Tokens.textSecondary; font: MV.Tokens.captionFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                            }
                            Label { text: "›"; color: MV.Tokens.textSecondary }
                        }
                        onClicked: { page.activeNeed = modelData; needPage.forceActiveFocus() }
                    }
                }
                RowLayout {
                    visible: careStore.busy
                    BusyIndicator { running: careStore.busy }
                    Label { text: Ts.get(TextId.Plants_Loading); Layout.fillWidth: true; wrapMode: Text.Wrap }
                }
                Label { text: careStore.error; color: MV.Tokens.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
                PageAction { text: Ts.get(TextId.Plants_Retry); visible: careStore.error.length > 0; enabled: !page.pending; Layout.fillWidth: true; onClicked: careStore.refresh() }
                PageAction { objectName: "addCareNeed"; text: "+  " + Ts.get(TextId.Care_Add); highlighted: true; enabled: !page.pending && !page.dirty && careStore.loaded && careStore.availableTypes.length > 0; Layout.fillWidth: true; onClicked: typeSelector.open() }
                Label { text: Ts.get(TextId.Care_AllAdded); visible: careStore.loaded && careStore.availableTypes.length === 0; color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
            Frame {
                Layout.fillWidth: true
                Layout.margins: MV.Tokens.space4
                ColumnLayout {
                    width: parent.width
                    spacing: MV.Tokens.space3
                    Label { text: Ts.get(TextId.Plants_ManagePlant); font: MV.Tokens.labelFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Label { text: Ts.get(TextId.Plants_SaveBeforeMove); visible: page.dirty; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Label { text: Ts.get(TextId.Plants_CurrentCollection) + " " + (page.plants.collections.find(c => c.id === page.specimen.collectionId)?.name ?? ""); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    ComboBox { id: destination; objectName: "moveDestination"; model: page.plants.collections; textRole: "name"; valueRole: "id"; Component.onCompleted: currentIndex = indexOfValue(page.specimen.collectionId); Accessible.name: Ts.get(TextId.Plants_Move); enabled: !page.pending && !page.dirty; Layout.fillWidth: true }
                    PageAction { objectName: "moveSpecimen"; text: Ts.get(TextId.Plants_Move); enabled: !page.pending && !page.dirty && destination.currentIndex >= 0 && destination.currentValue !== page.specimen.collectionId; Layout.fillWidth: true; onClicked: page.submit(PlantModel.MoveSpecimen) }
                    PageAction { objectName: "archiveSpecimen"; text: Ts.get(page.specimen.archived ? TextId.Plants_Restore : TextId.Plants_Archive); enabled: !page.pending && !page.dirty; Layout.fillWidth: true; onClicked: page.submit(page.specimen.archived ? PlantModel.RestoreSpecimen : PlantModel.ArchiveSpecimen) }
                }
            }
        }
    }
    CareNeedPage { id: needPage; objectName: "careNeedDetails"; anchors.fill: parent; visible: page.activeNeed !== null; care: careStore; need: page.activeNeed ?? ({}); onCloseRequested: { page.activeNeed = null; page.forceActiveFocus() } }
    Dialog {
        id: typeSelector
        objectName: "careTypeSelector"
        anchors.centerIn: parent
        width: Math.min(page.width - MV.Tokens.space6, 440)
        height: Math.min(page.height - MV.Tokens.space6, 640)
        modal: true
        closePolicy: careStore.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
        title: Ts.get(TextId.Care_Select)
        ColumnLayout {
            anchors.fill: parent
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    Repeater {
                        model: careStore.availableTypes
                        delegate: ItemDelegate {
                            required property var modelData
                            objectName: "addNeed_" + modelData.type
                            text: modelData.name
                            icon.source: modelData.icon
                            icon.color: MV.Tokens.accent
                            Layout.fillWidth: true
                            enabled: !careStore.busy
                            onClicked: careStore.add(modelData.type)
                        }
                    }
                }
            }
            Label { text: Ts.get(TextId.Plants_Loading); visible: careStore.busy; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { text: careStore.error; color: MV.Tokens.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.Wrap }
            PageAction { text: Ts.get(TextId.Plants_Back); enabled: !careStore.busy; Layout.fillWidth: true; onClicked: typeSelector.close() }
        }
    }
    Dialog {
        id: discard
        objectName: "discardConfirmation"
        onClosed: page.discardContinuation = null
        anchors.centerIn: parent
        width: Math.min(page.width - MV.Tokens.space6, 440)
        modal: true
        title: Ts.get(TextId.Plants_Discard)
        height: Math.min(implicitHeight, page.height - MV.Tokens.space6)
        ScrollView {
            anchors.fill: parent
            contentWidth: availableWidth
            implicitHeight: discardContent.implicitHeight
            ColumnLayout {
                id: discardContent
                width: parent.width
                Label { text: Ts.get(TextId.Specimen_DiscardConfirm); Layout.fillWidth: true; wrapMode: Text.Wrap }
                PageAction {
                    objectName: "confirmDiscard"
                    text: Ts.get(TextId.Plants_Discard)
                    Layout.fillWidth: true
                    onClicked: {
                        const proceed = page.discardContinuation
                        page.discardContinuation = null
                        page.discardEdits()
                        discard.close()
                        if (proceed) proceed()
                    }
                }
                PageAction { objectName: "cancelDiscard"; text: Ts.get(TextId.Plants_Back); Layout.fillWidth: true; onClicked: discard.close() }
            }
        }
    }
}
