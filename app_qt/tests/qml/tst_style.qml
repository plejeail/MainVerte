import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels
import QtTest
import MainVerteStyle as MV
import MainVerte.Showcase
import MainVerte as App

TestCase {
    id: testCase
    name: "MainVerteStyle"
    width: 1000
    height: 800
    visible: true
    when: windowShown

    Component { id: buttonComponent; Button { text: "Water fern"; checkable: true } }
    Component { id: checkboxComponent; CheckBox { text: "Reminder"; tristate: true } }
    Component { id: switchComponent; Switch { text: "Reminders" } }
    Component { id: sliderComponent; Slider { from: 0; to: 10; stepSize: 1; value: 5 } }
    Component { id: spinComponent; SpinBox { from: 0; to: 10; value: 5; editable: true } }
    Component { id: inputComponent; TextField { validator: IntValidator { bottom: 1; top: 30 } } }
    Component { id: comboComponent; ComboBox { model: ["Fern", "Pilea", "Monstera"] } }
    Component { id: galleryComponent; Showcase { } }
    Component { id: mainComponent; App.Main { } }
    Component { id: progressComponent; ProgressBar { width: 200; value: 0.6; indeterminate: true } }
    Component {
        id: modelViewsComponent
        Item {
            width: 800
            height: 400
            HorizontalHeaderView { id: horizontalHeader; syncView: table; width: 360 }
            VerticalHeaderView { syncView: table; height: 150; y: 40 }
            TableView {
                id: table
                x: 40; y: 40; width: 320; height: 150
                model: TableModel {
                    TableModelColumn { display: "name" }
                    TableModelColumn { display: "humidity" }
                    rows: [{name: "Fern", humidity: "72 %"}, {name: "Monstera", humidity: "64 %"}]
                }
                delegate: TableViewDelegate { implicitWidth: 150 }
            }
            TreeView {
                id: tree
                x: 400; width: 360; height: 200
                model: TreeModel {
                    TableModelColumn { display: "name" }
                    rows: [{name: "Indoor", rows: [{name: "Fern"}, {name: "Pilea"}]}]
                }
                delegate: TreeViewDelegate { implicitWidth: 180 }
                Component.onCompleted: expand(0)
            }
        }
    }

    function init() { failOnWarning(/.*/); MV.Tokens.dark = false; MV.Tokens.fontScale = 1 }
    function cleanup() { MV.Tokens.dark = false; MV.Tokens.fontScale = 1 }

    function test_main_window_resize() {
        const window = createTemporaryObject(mainComponent, null, {settings: {}})
        verify(window)
        tryCompare(window, "visible", true)
        if (!MV.Tokens.touch) {
            compare(window.width, 960)
            compare(window.height, 640)
            verify(window.desktopNavigation)
        }
        for (const width of [900, 719, 720, 390, 1000, 640, 960]) {
            window.width = width
            window.height = 700
            wait(30)
            compare(window.width, width)
            compare(window.height, 700)
            compare(window.desktopNavigation, width >= MV.Tokens.compactBreakpoint)
        }
        window.close()
    }

    function test_main_window_passes_settings_to_page() {
        const settings = {
            languageIndex: 0, languages: [{label: "English", flag: ""}],
            saving: false, darkTheme: false, saveFailed: false,
            selectLanguage: function(index) { settings.selectedLanguage = index },
            selectTheme: function(dark) { settings.selectedTheme = dark }
        }
        const window = createTemporaryObject(mainComponent, null, {settings: settings})
        verify(window)
        window.showSection(1)
        const language = findChild(window, "languageSelector")
        verify(language)
        compare(language.currentIndex, 0)
        language.activated(0)
        compare(settings.selectedLanguage, 0)
        const theme = findChild(window, "themeSelector")
        verify(theme)
        theme.activated(1)
        compare(settings.selectedTheme, true)
        window.close()
    }

    function test_progress_animation_transitions_data() {
        return [{tag: "determinate"}, {tag: "hidden"}]
    }
    function test_progress_animation_transitions(data) {
        const progress = createTemporaryObject(progressComponent, testCase)
        const fill = progress.contentItem.children[0]
        function pulsing() { return fill.opacity > 0.4 && fill.opacity < 0.9 }
        function expectPulsing() {
            tryVerify(pulsing, 2000)
            const previousOpacity = fill.opacity
            tryVerify(function() { return Math.abs(fill.opacity - previousOpacity) > 0.01 }, 2000)
        }
        function stop() {
            if (data.tag === "determinate") progress.indeterminate = false
            else progress.visible = false
        }
        function restart() {
            progress.indeterminate = true
            progress.visible = true
        }
        expectPulsing()
        stop()
        compare(fill.opacity, 1)
        wait(80)
        compare(fill.opacity, 1)
        if (data.tag === "determinate") compare(fill.width, progress.position * progress.contentItem.width)
        restart()
        expectPulsing()
        stop()
        compare(fill.opacity, 1)
    }

    function test_load_style_data() { return [{tag: "light", dark: false}, {tag: "dark", dark: true}] }
    function test_load_style(data) {
        MV.Tokens.dark = data.dark
        const types = ["Button", "DelayButton", "RoundButton", "ToolButton", "CheckBox", "RadioButton", "Switch", "SwitchDelegate", "CheckDelegate", "RadioDelegate", "ItemDelegate", "MenuItem", "TextField", "TextArea", "SpinBox", "DoubleSpinBox", "ComboBox", "SearchField", "Slider", "RangeSlider", "Dial", "Tumbler", "ScrollBar", "ScrollIndicator", "TabBar", "TabButton", "ToolBar", "MenuBar", "MenuBarItem", "Menu", "MenuSeparator", "ToolSeparator", "Drawer", "SwipeDelegate", "SwipeView", "PageIndicator", "SplitView", "StackView", "Pane", "Frame", "GroupBox", "Page", "ApplicationWindow", "Dialog", "DialogButtonBox", "Popup", "ToolTip", "ProgressBar", "BusyIndicator", "ScrollView", "Label", "Control", "Container", "AbstractButton", "HorizontalHeaderView", "VerticalHeaderView", "SelectionRectangle", "DayOfWeekRow", "MonthGrid", "WeekNumberColumn"]
        for (const type of types) {
            const component = Qt.createComponent("data:text/plain,import QtQuick; import QtQuick.Controls; " + type + " {}")
            compare(component.status, Component.Ready, type + ": " + component.errorString())
            const object = createTemporaryObject(component, testCase)
            verify(object !== null, type)
            if (object.font !== undefined) compare(object.font.family, MV.Tokens.fontFamily, type)
            object.destroy()
            component.destroy()
        }
    }

    function test_button_keyboard_and_disabled() {
        const button = createTemporaryObject(buttonComponent, testCase)
        verify(button.background.toString().indexOf("ControlSurface") >= 0, "Custom style is discovered")
        button.forceActiveFocus(Qt.TabFocusReason)
        verify(button.visualFocus)
        keyClick(Qt.Key_Space)
        verify(button.checked)
        button.enabled = false
        mouseClick(button)
        verify(button.checked)
    }
    function test_selection() {
        const check = createTemporaryObject(checkboxComponent, testCase)
        check.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(check.checkState, Qt.PartiallyChecked)
        keyClick(Qt.Key_Space)
        compare(check.checkState, Qt.Checked)
        const toggle = createTemporaryObject(switchComponent, testCase, {y: 100})
        mouseClick(toggle)
        verify(toggle.checked)
    }
    function test_numeric_keyboard() {
        const slider = createTemporaryObject(sliderComponent, testCase)
        slider.forceActiveFocus()
        keyClick(Qt.Key_Right)
        compare(slider.value, 6)
        const spin = createTemporaryObject(spinComponent, testCase)
        spin.forceActiveFocus()
        keyClick(Qt.Key_Up)
        compare(spin.value, 6)
        verify(spin.up.indicator.enabled)
        spin.value = spin.to
        verify(!spin.up.indicator.enabled)
        verify(spin.down.indicator.enabled)
    }
    function test_text_and_combo() {
        const input = createTemporaryObject(inputComponent, testCase)
        input.text = "31"
        verify(!input.acceptableInput)
        input.text = "12"
        verify(input.acceptableInput)
        const combo = createTemporaryObject(comboComponent, testCase, {y: 100})
        combo.forceActiveFocus()
        keyClick(Qt.Key_Down)
        compare(combo.currentIndex, 1)
        combo.popup.open()
        tryCompare(combo.popup, "visible", true)
        keyClick(Qt.Key_Escape)
        tryCompare(combo.popup, "visible", false)
    }
    function test_theme_updates_existing_controls() {
        const button = createTemporaryObject(buttonComponent, testCase, {highlighted: true})
        const input = createTemporaryObject(inputComponent, testCase)
        const before = String(button.background.color)
        MV.Tokens.dark = true
        compare(button.background.color, MV.Tokens.accent)
        compare(button.palette.brightText, MV.Tokens.textOnAccent)
        compare(button.contentItem.color, MV.Tokens.textOnAccent)
        verify(String(button.background.color) !== before)
        compare(input.color, MV.Tokens.text)
        compare(input.background.color, MV.Tokens.elevated)
        MV.Tokens.dark = false
        compare(button.palette.brightText, MV.Tokens.textOnAccent)
        compare(button.contentItem.color, MV.Tokens.textOnAccent)
    }
    function test_model_views_data() { return [{tag: "light", dark: false}, {tag: "dark", dark: true}] }
    function test_model_views(data) {
        MV.Tokens.dark = data.dark
        const views = createTemporaryObject(modelViewsComponent, testCase)
        verify(views)
        waitForRendering(views)
    }
    function luminance(color) {
        function linear(v) { return v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4) }
        return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b)
    }
    function contrast(a, b) { const x = luminance(a), y = luminance(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05) }
    function test_contrast_data() { return [{tag: "light", dark: false}, {tag: "dark", dark: true}] }
    function test_contrast(data) {
        MV.Tokens.dark = data.dark
        const t = MV.Tokens
        for (const foreground of [t.text, t.textSecondary, t.textTertiary, t.success, t.warning, t.error, t.information, t.secondaryAccent]) {
            for (const background of [t.background, t.surface, t.elevated, t.surfaceSecondary]) {
                verify(contrast(foreground, background) >= 4.5, foreground + " on " + background)
            }
        }
        for (const fill of [t.accent, t.accentHover, t.accentPressed]) verify(contrast(t.textOnAccent, fill) >= 4.5, String(t.textOnAccent) + " on " + String(fill) + ": " + contrast(t.textOnAccent, fill))
        for (const background of [t.background, t.surface, t.elevated, t.surfaceSecondary]) {
            verify(contrast(t.border, background) >= 3, "Control boundary contrast")
            verify(contrast(t.focus, background) >= 3, "Focus contrast")
        }
    }
    function test_showcase_data() {
        return [
            {tag: "desktop-light", width: 1180, height: 880, dark: false, scale: 1},
            {tag: "desktop-dark", width: 1180, height: 880, dark: true, scale: 1},
            {tag: "phone-light", width: 390, height: 844, dark: false, scale: 1},
            {tag: "phone-dark", width: 390, height: 844, dark: true, scale: 1},
            {tag: "short-window", width: 640, height: 480, dark: false, scale: 1},
            {tag: "large-type", width: 390, height: 844, dark: false, scale: 1.5}
        ]
    }
    function test_showcase(data) {
        MV.Tokens.dark = data.dark
        MV.Tokens.fontScale = data.scale
        const gallery = createTemporaryObject(galleryComponent, null, {width: data.width, height: data.height})
        verify(gallery)
        waitForRendering(gallery.contentItem)
        const toggle = findChild(gallery, "themeSwitch")
        verify(toggle)
        compare(toggle.checked, data.dark)
        const dialog = findChild(gallery, "plantDialog")
        dialog.open()
        tryCompare(dialog, "opened", true)
        verify(dialog.height <= gallery.height - MV.Tokens.margin * 2)
        const field = findChild(gallery, "registrationName")
        verify(!field.acceptableInput)
        verify(!dialog.footer.standardButton(Dialog.Save).enabled)
        field.text = "Boston fern"
        verify(field.acceptableInput)
        verify(dialog.footer.standardButton(Dialog.Save).enabled)
        mouseClick(dialog.footer.standardButton(Dialog.Save))
        tryCompare(dialog, "visible", false)
        verify(gallery.registered)
        gallery.close()
    }
}
