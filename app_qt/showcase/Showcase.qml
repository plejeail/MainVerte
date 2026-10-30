pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels
import MainVerteStyle as MV

ApplicationWindow {
    id: window
    width: 1180
    height: 880
    minimumWidth: 320
    minimumHeight: 480
    visible: true
    title: qsTr("MainVerte · Design system")
    readonly property bool compact: width < MV.Tokens.compactBreakpoint
    property bool registered: false

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: MV.Tokens.space3
            Label { text: "mainverte"; font: MV.Tokens.titleFont; Layout.fillWidth: true }
            Label { text: qsTr("COMPONENT GARDEN / 01"); font: MV.Tokens.captionFont; color: MV.Tokens.textSecondary; visible: !window.compact }
            Switch {
                objectName: "themeSwitch"
                text: qsTr("Dark theme")
                checked: MV.Tokens.dark
                onToggled: MV.Tokens.dark = checked
            }
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        background: Rectangle { color: MV.Tokens.background }
        ColumnLayout {
            width: scroll.availableWidth
            spacing: MV.Tokens.space5
            Item { Layout.preferredHeight: MV.Tokens.space2 }
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(MV.Tokens.contentWidth, scroll.availableWidth - MV.Tokens.margin * 2)
                Layout.fillWidth: true
                Layout.maximumWidth: MV.Tokens.contentWidth
                Layout.leftMargin: MV.Tokens.margin
                Layout.rightMargin: MV.Tokens.margin
                spacing: MV.Tokens.space5

                Label { text: qsTr("ROOTED IN OBSERVATION"); font: MV.Tokens.captionFont; color: MV.Tokens.secondaryAccent }
                Label {
                    text: qsTr("A little closer to nature.")
                    font: window.compact ? MV.Tokens.headingFont : MV.Tokens.displayFont
                    lineHeight: MV.Tokens.headingLineHeight
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                Label {
                    text: qsTr("A calm, considered toolkit for the everyday care of growing things.")
                    color: MV.Tokens.textSecondary
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }

                GridLayout {
                    columns: window.compact ? 1 : 2
                    columnSpacing: MV.Tokens.space5
                    rowSpacing: MV.Tokens.space5
                    Layout.fillWidth: true
                    Section {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        title: qsTr("Monstera deliciosa")
                        caption: qsTr("01 / THE LIVING ROOM · Joined your collection in April")
                        Flow {
                            Layout.fillWidth: true
                            spacing: MV.Tokens.space2
                            StatusBadge { text: qsTr("Healthy growth"); statusColor: MV.Tokens.healthy }
                            StatusBadge { text: qsTr("Indirect sunlight"); statusColor: MV.Tokens.sunlight }
                        }
                        Label { text: qsTr("A new leaf is on its way."); font: MV.Tokens.titleFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        Label { text: qsTr("Keep the soil lightly moist and turn the pot a little each week."); color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        Flow {
                            Layout.fillWidth: true
                            spacing: MV.Tokens.space5
                            Measurement { value: "22 °C"; caption: qsTr("Air temperature"); measurementColor: MV.Tokens.soil }
                            Measurement { value: "64 %"; caption: qsTr("Humidity"); measurementColor: MV.Tokens.moisture }
                            Measurement { value: "1.2k lx"; caption: qsTr("Light level"); measurementColor: MV.Tokens.sunlight }
                        }
                    }
                    Section {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        title: qsTr("A moment of care")
                        caption: qsTr("TODAY / CARE REMINDER")
                        StatusBadge { text: qsTr("Watering due"); statusColor: MV.Tokens.warning }
                        Label { text: qsTr("Check your fern’s soil"); font: MV.Tokens.headingFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        Label { text: qsTr("Boston fern · Bedroom\nWater when the top layer feels dry."); color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        Button { text: checked ? qsTr("Care recorded") : qsTr("Mark as cared for"); highlighted: true; checkable: true; Layout.fillWidth: window.compact }
                    }
                }

                Section {
                    title: qsTr("01 / Typography & color")
                    caption: qsTr("System typography, botanical color, and a warm mineral foundation.")
                    Layout.fillWidth: true
                    Label { text: qsTr("Growing with intention"); font: MV.Tokens.headingFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Label { text: qsTr("Title · Your personal plant journal"); font: MV.Tokens.titleFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Label { text: qsTr("Body · Observe the small changes. They tell a bigger story."); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Label { text: qsTr("Supporting text · Last observed today at 09:30"); font: MV.Tokens.supportingFont; color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Flow {
                        Layout.fillWidth: true
                        spacing: MV.Tokens.space3
                        Repeater {
                            model: [qsTr("Botanical"), qsTr("Terracotta"), qsTr("Moisture"), qsTr("Sunlight"), qsTr("Healthy"), qsTr("Needs care")]
                            Column {
                                id: swatchColumn
                                required property int index
                                required property string modelData
                                spacing: MV.Tokens.space2
                                Rectangle {
                                    width: MV.Tokens.controlHeight * 2
                                    height: MV.Tokens.space6
                                    radius: MV.Tokens.radiusSmall
                                    color: [MV.Tokens.accent, MV.Tokens.secondaryAccent, MV.Tokens.moisture, MV.Tokens.sunlight, MV.Tokens.success, MV.Tokens.error][swatchColumn.index]
                                }
                                Label { text: swatchColumn.modelData; font: MV.Tokens.captionFont }
                            }
                        }
                    }
                }

                Section {
                    title: qsTr("02 / Buttons & actions")
                    caption: qsTr("Primary, secondary, quiet, selected, disabled, and hold-to-confirm. Tab to inspect focus; hover or press to inspect feedback.")
                    Layout.fillWidth: true
                    Flow {
                        Layout.fillWidth: true
                        spacing: MV.Tokens.space3
                        Button { text: qsTr("Add a plant"); highlighted: true; onClicked: plantDialog.open() }
                        Button { text: qsTr("View journal") }
                        Button { text: qsTr("Learn more"); flat: true }
                        Button { text: qsTr("Selected"); checkable: true; checked: true }
                        Button { text: qsTr("Unavailable"); enabled: false }
                        RoundButton { text: "+"; Accessible.name: qsTr("Add observation") }
                        ToolButton { text: qsTr("Options"); onClicked: optionsMenu.popup(); ToolTip.visible: hovered; ToolTip.text: qsTr("More plant actions") }
                        DelayButton { text: checked ? qsTr("Confirmed") : qsTr("Hold to confirm"); delay: 1200 }
                    }
                }

                GridLayout {
                    columns: window.compact ? 1 : 2
                    columnSpacing: MV.Tokens.space5
                    rowSpacing: MV.Tokens.space5
                    Layout.fillWidth: true
                    Section {
                        title: qsTr("03 / Text & input")
                        caption: qsTr("Persistent labels, clear validation, and comfortable editing.")
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        Label { text: qsTr("Plant nickname"); font: MV.Tokens.labelFont }
                        TextField { placeholderText: qsTr("e.g. Sunday fern"); Layout.fillWidth: true; Accessible.name: qsTr("Plant nickname") }
                        Label { text: qsTr("Observation"); font: MV.Tokens.labelFont }
                        TextArea { placeholderText: qsTr("What has changed since last week?"); wrapMode: TextEdit.Wrap; Layout.fillWidth: true; Accessible.name: qsTr("Observation") }
                        Label { text: qsTr("Plant name · required"); font: MV.Tokens.labelFont }
                        MV.TextField { invalid: true; placeholderText: qsTr("Botanical or common name"); Layout.fillWidth: true; Accessible.name: qsTr("Plant name, required"); Accessible.description: qsTr("Enter a name to continue") }
                        Label { text: qsTr("Enter a name to continue."); color: MV.Tokens.error; font: MV.Tokens.captionFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        ComboBox { model: [qsTr("Indirect sunlight"), qsTr("Full sun"), qsTr("Low light")]; Layout.fillWidth: true; Accessible.name: qsTr("Light preference") }
                        SearchField { placeholderText: qsTr("Search your plants"); suggestionModel: ["Monstera", "Fern", "Pilea"]; Layout.fillWidth: true; Accessible.name: qsTr("Search plants") }
                        Flow {
                            Layout.fillWidth: true
                            spacing: MV.Tokens.space3
                            SpinBox { from: 1; to: 30; value: 7; editable: true; Accessible.name: qsTr("Days between watering") }
                            DoubleSpinBox { from: 0; to: 14; value: 6.5; decimals: 1; stepSize: 0.1; editable: true; Accessible.name: qsTr("Soil pH") }
                        }
                        TextField { text: qsTr("Read-only observation"); readOnly: true; Layout.fillWidth: true; Accessible.name: text }
                        TextField { placeholderText: qsTr("Disabled field"); enabled: false; Layout.fillWidth: true }
                    }
                    Section {
                        title: qsTr("04 / Selection & settings")
                        caption: qsTr("Shared indicators, useful hit areas, and native keyboard behavior.")
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        CheckBox { text: qsTr("Notify me about watering"); checked: true }
                        CheckBox { text: qsTr("Some plants selected"); tristate: true; checkState: Qt.PartiallyChecked }
                        CheckBox { text: qsTr("Notifications unavailable"); enabled: false; checked: true }
                        ColumnLayout {
                            RadioButton { text: qsTr("Weekly observations"); checked: true }
                            RadioButton { text: qsTr("Every two weeks") }
                        }
                        Switch { text: qsTr("Care reminders"); checked: true }
                        SwitchDelegate { text: qsTr("Show environmental data"); checked: true; Layout.fillWidth: true }
                        CheckDelegate { text: qsTr("Include archived plants"); Layout.fillWidth: true }
                        RadioDelegate { text: qsTr("Metric measurements"); checked: true; Layout.fillWidth: true }
                        Label { text: qsTr("Moisture threshold"); font: MV.Tokens.labelFont }
                        Slider { value: 0.6; Layout.fillWidth: true; Accessible.name: qsTr("Moisture threshold") }
                        Label { text: qsTr("Comfortable humidity range"); font: MV.Tokens.labelFont }
                        RangeSlider { first.value: 0.35; second.value: 0.75; Layout.fillWidth: true; Accessible.name: qsTr("Humidity range") }
                        Flow {
                            Layout.fillWidth: true
                            spacing: MV.Tokens.space5
                            Dial { value: 0.4; Accessible.name: qsTr("Misting intensity") }
                            Tumbler { model: [qsTr("Morning"), qsTr("Afternoon"), qsTr("Evening")]; visibleItemCount: 3; implicitHeight: MV.Tokens.controlHeight * 3; implicitWidth: MV.Tokens.controlHeight * 3; Accessible.name: qsTr("Reminder time") }
                        }
                    }
                }

                Section {
                    title: qsTr("05 / Navigation")
                    caption: qsTr("Tabs, menus, list rows, swipe actions, and page indicators.")
                    Layout.fillWidth: true
                    TabBar {
                        id: exampleTabs
                        Layout.fillWidth: true
                        TabButton { text: qsTr("Overview") }
                        TabButton { text: qsTr("Care") }
                        TabButton { text: qsTr("Journal") }
                    }
                    SwipeView {
                        id: exampleSwipe
                        currentIndex: exampleTabs.currentIndex
                        onCurrentIndexChanged: exampleTabs.currentIndex = currentIndex
                        implicitHeight: MV.Tokens.controlHeight * 2
                        Layout.fillWidth: true
                        clip: true
                        Label { text: qsTr("Your plants, at a glance."); padding: MV.Tokens.space4; wrapMode: Text.Wrap }
                        Label { text: qsTr("Small rituals. Healthy growth."); padding: MV.Tokens.space4; wrapMode: Text.Wrap }
                        Label { text: qsTr("A record of everything you notice."); padding: MV.Tokens.space4; wrapMode: Text.Wrap }
                    }
                    PageIndicator { count: exampleSwipe.count; currentIndex: exampleSwipe.currentIndex; interactive: true; onCurrentIndexChanged: exampleSwipe.setCurrentIndex(currentIndex); Layout.alignment: Qt.AlignHCenter }
                    ItemDelegate { text: qsTr("Living room · 6 plants"); Layout.fillWidth: true }
                    ItemDelegate { text: qsTr("Bedroom · selected"); highlighted: true; Layout.fillWidth: true }
                    SwipeDelegate {
                        id: swipeRow
                        text: qsTr("Fern · swipe left for care action")
                        Layout.fillWidth: true
                        swipe.right: Button { width: swipeRow.width / 2; height: swipeRow.height; text: qsTr("Record care"); highlighted: true; onClicked: swipeRow.swipe.close() }
                    }
                    MenuBar {
                        Layout.fillWidth: true
                        Menu {
                            title: qsTr("Collection")
                            MenuItem { text: qsTr("Add a plant"); onTriggered: plantDialog.open() }
                            MenuSeparator { }
                            MenuItem { text: qsTr("Include archived"); checkable: true }
                        }
                        Menu { title: qsTr("View"); MenuItem { text: qsTr("Open navigation drawer"); onTriggered: exampleDrawer.open() } }
                    }
                }

                Section {
                    title: qsTr("06 / Containers")
                    caption: qsTr("Quiet surface layers, resizable regions, and a brief stack transition.")
                    Layout.fillWidth: true
                    GroupBox {
                        title: qsTr("Plant registration")
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            Label { text: qsTr("Give your new plant a place in the collection."); Layout.fillWidth: true; wrapMode: Text.Wrap }
                            Button { text: qsTr("Register a plant"); onClicked: plantDialog.open() }
                        }
                    }
                    Frame { Layout.fillWidth: true; Label { text: qsTr("Frame · A quiet boundary for related observations."); width: parent.width; wrapMode: Text.Wrap } }
                    SplitView {
                        Layout.fillWidth: true
                        implicitHeight: MV.Tokens.controlHeight * 3
                        Pane { SplitView.minimumWidth: 60; SplitView.preferredWidth: window.compact ? 110 : 240; Label { text: qsTr("Collection"); anchors.fill: parent; wrapMode: Text.Wrap } }
                        Pane { SplitView.minimumWidth: 60; SplitView.fillWidth: true; Label { text: qsTr("Drag the divider to resize."); anchors.fill: parent; wrapMode: Text.Wrap } }
                    }
                    StackView {
                        id: exampleStack
                        Layout.fillWidth: true
                        implicitHeight: MV.Tokens.controlHeight * 2
                        initialItem: Component { Button { text: qsTr("Open observation"); onClicked: exampleStack.push(observation) } }
                    }
                    Component { id: observation; Button { text: qsTr("Back to collection"); onClicked: exampleStack.pop() } }
                }

                Section {
                    title: qsTr("07 / Dialogs & popups")
                    caption: qsTr("Clear decisions, anchored menus, and lightweight supporting information.")
                    Layout.fillWidth: true
                    Flow {
                        Layout.fillWidth: true
                        spacing: MV.Tokens.space3
                        Button { text: qsTr("Plant registration form"); onClicked: plantDialog.open() }
                        Button { text: qsTr("Open popup"); onClicked: notePopup.open() }
                        Button { text: qsTr("Open drawer"); onClicked: exampleDrawer.open() }
                        Button { text: qsTr("Show tooltip"); ToolTip.text: qsTr("Keep observations brief and regular."); ToolTip.visible: hovered || down || visualFocus }
                    }
                    Label { text: window.registered ? qsTr("Plant registered in this demo.") : qsTr("Demo actions use temporary state."); color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                }

                Section {
                    title: qsTr("08 / Progress & feedback")
                    caption: qsTr("Status always includes a text label; color reinforces its meaning.")
                    Layout.fillWidth: true
                    Label { text: qsTr("Care plan · 3 of 5 observations recorded") }
                    ProgressBar { value: 0.6; Layout.fillWidth: true; Accessible.name: qsTr("Care plan, 60 percent complete") }
                    Label { text: qsTr("Reading environmental measurements…"); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    ProgressBar { indeterminate: true; Layout.fillWidth: true; Accessible.name: qsTr("Reading measurements") }
                    BusyIndicator { running: true; Accessible.name: qsTr("Loading measurements") }
                    Flow {
                        Layout.fillWidth: true
                        spacing: MV.Tokens.space2
                        StatusBadge { text: qsTr("Healthy"); statusColor: MV.Tokens.success }
                        StatusBadge { text: qsTr("Water soon"); statusColor: MV.Tokens.warning }
                        StatusBadge { text: qsTr("Needs attention"); statusColor: MV.Tokens.error }
                        StatusBadge { text: qsTr("Observation saved"); statusColor: MV.Tokens.information }
                    }
                }

                Section {
                    title: qsTr("09 / Models & calendar")
                    caption: qsTr("Qt 6.12 table delegates, headers, and calendar primitives share the same palette.")
                    Layout.fillWidth: true
                    HorizontalHeaderView { id: tableHeader; syncView: readings; Layout.fillWidth: true; implicitHeight: MV.Tokens.controlHeight }
                    TableView {
                        id: readings
                        Layout.fillWidth: true
                        implicitHeight: MV.Tokens.controlHeight * 3
                        clip: true
                        columnWidthProvider: function(column) { return width / 2 }
                        model: TableModel {
                            TableModelColumn { display: "plant" }
                            TableModelColumn { display: "humidity" }
                            rows: [{plant: "Monstera", humidity: "64 %"}, {plant: "Boston fern", humidity: "72 %"}]
                        }
                        delegate: TableViewDelegate { }
                        selectionModel: ItemSelectionModel { }
                        SelectionRectangle { target: readings }
                    }
                    DayOfWeekRow { locale: calendar.locale; Layout.fillWidth: true }
                    MonthGrid { id: calendar; month: 3; year: 2026; Layout.fillWidth: true; implicitHeight: MV.Tokens.controlHeight * 5 }
                }
                Label { text: qsTr("MAINVERTE / A considered space for growing things."); font: MV.Tokens.captionFont; color: MV.Tokens.textSecondary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Item { Layout.preferredHeight: MV.Tokens.space5 }
            }
        }
    }

    Menu {
        id: optionsMenu
        MenuItem { text: qsTr("Add observation"); onTriggered: notePopup.open() }
        MenuItem { text: qsTr("Show reminders"); checkable: true; checked: true }
        MenuSeparator { }
        MenuItem { text: qsTr("Unavailable action"); enabled: false }
    }
    Drawer {
        id: exampleDrawer
        width: Math.min(window.width * 0.85, 340)
        height: window.height
        ColumnLayout {
            anchors.fill: parent
            Label { text: "mainverte"; font: MV.Tokens.headingFont }
            ItemDelegate { text: qsTr("Collection"); highlighted: true; Layout.fillWidth: true; onClicked: exampleDrawer.close() }
            ItemDelegate { text: qsTr("Care journal"); Layout.fillWidth: true; onClicked: exampleDrawer.close() }
            Item { Layout.fillHeight: true }
            Button { text: qsTr("Close"); Layout.fillWidth: true; onClicked: exampleDrawer.close() }
        }
    }
    Popup {
        id: notePopup
        anchors.centerIn: parent
        width: Math.min(window.width - MV.Tokens.margin * 2, 360)
        modal: true
        focus: true
        ColumnLayout {
            width: parent.width
            Label { text: qsTr("A small observation"); font: MV.Tokens.titleFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
            Label { text: qsTr("New growth is a useful sign that your plant is settling into its space."); Layout.fillWidth: true; wrapMode: Text.Wrap }
            Button { text: qsTr("Got it"); onClicked: notePopup.close() }
        }
    }
    Dialog {
        id: plantDialog
        objectName: "plantDialog"
        anchors.centerIn: parent
        width: Math.min(window.width - MV.Tokens.margin * 2, 460)
        height: Math.min(implicitHeight, window.height - MV.Tokens.margin * 2)
        title: qsTr("Welcome a new plant")
        modal: true
        focus: true
        standardButtons: Dialog.Save | Dialog.Cancel
        onOpened: nameField.forceActiveFocus()
        onAccepted: window.registered = true
        footer: DialogButtonBox {
            standardButtons: Dialog.Save | Dialog.Cancel
            Component.onCompleted: standardButton(Dialog.Save).enabled = Qt.binding(function() { return nameField.acceptableInput })
        }
        contentItem: ScrollView {
            id: formScroll
            implicitHeight: registrationForm.implicitHeight
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                id: registrationForm
                width: formScroll.availableWidth
                spacing: MV.Tokens.space3
                Label { text: qsTr("Plant name · required"); font: MV.Tokens.labelFont }
                TextField {
                    id: nameField
                    objectName: "registrationName"
                    Layout.fillWidth: true
                    placeholderText: qsTr("e.g. Boston fern")
                    validator: RegularExpressionValidator { regularExpression: /.*\S.*/ }
                    Accessible.name: qsTr("Plant name, required")
                }
                Label { text: qsTr("Location"); font: MV.Tokens.labelFont }
                ComboBox { Layout.fillWidth: true; model: [qsTr("Living room"), qsTr("Bedroom"), qsTr("Balcony")]; Accessible.name: qsTr("Plant location") }
                Label { text: qsTr("Notes"); font: MV.Tokens.labelFont }
                TextArea { Layout.fillWidth: true; placeholderText: qsTr("A few words about your new arrival"); wrapMode: TextEdit.Wrap; Accessible.name: qsTr("Plant notes") }
            }
        }
    }
}
