import QtQuick
import QtQuick.Controls
import QtTest
import MainVerte as App
import MainVerteStyle as MV

TestCase {
    id: test
    name: "SpecimenCare"
    width: 960
    height: 800
    visible: true
    when: windowShown
    Component { id: pageComponent; App.CollectionPage { width: test.width; height: test.height } }
    function settle(model) { tryCompare(model, "busy", false); compare(model.error, "") }
    function init() { journalTestSetup.resetJournal() }
    function cleanup() { MV.Tokens.dark = false; MV.Tokens.fontScale = 1 }
    Component { id: mainComponent; App.Main {} }
    function test_discard_navigation_data() {
        return [{tag: "mobile", width: 393}, {tag: "desktop", width: 960}]
    }
    function test_discard_navigation(data) {
        failOnWarning(/.*/)
        const settings = { languageIndex: 0, languages: [{label: "English", flag: ""}], saving: false,
            darkTheme: false, saveFailed: false, logLevel: 0, debugEnabled: false,
            softwareVersion: "0.1.0", databaseVersion: 2 }
        const window = createTemporaryObject(mainComponent, null, {settings: settings, width: data.width})
        verify(window)
        const plants = findChild(window, "plantModel")
        settle(plants)
        plants.submit(5, plants.selected, "Nishiro")
        settle(plants)
        const grid = findChild(window, "specimenGrid")
        tryCompare(grid, "count", 1)
        grid.itemAtIndex(0).openDetails()
        let details = findChild(window, "specimenDetails")
        settle(findChild(details, "careModel"))
        const care = findChild(details, "careModel")
        care.refresh()
        verify(care.busy)
        window.showSection(1)
        compare(window.currentSection, 0)
        compare(findChild(details, "discardConfirmation").visible, false)
        settle(care)
        findChild(details, "editSpecimen").clicked()
        findChild(details, "specimenName").text = "Unsaved Nishiro"
        const navigation = findChild(window, data.width < MV.Tokens.compactBreakpoint ? "settingsTab" : "settingsNavigation")
        mouseClick(navigation)
        let confirmation = findChild(details, "discardConfirmation")
        tryCompare(confirmation, "visible", true)
        compare(window.currentSection, 0)
        findChild(confirmation.contentItem, "cancelDiscard").clicked()
        tryCompare(confirmation, "visible", false)
        compare(window.currentSection, 0)
        compare(findChild(details, "specimenName").text, "Unsaved Nishiro")
        verify(details.editing)
        compare(navigation.checked, false)
        // A normal back request after cancellation must not retain the Settings destination.
        details.close()
        tryCompare(confirmation, "visible", true)
        findChild(confirmation.contentItem, "confirmDiscard").clicked()
        tryVerify(() => findChild(window, "specimenDetails") === null)
        compare(window.currentSection, 0)
        grid.itemAtIndex(0).openDetails()
        details = findChild(window, "specimenDetails")
        settle(findChild(details, "careModel"))
        findChild(details, "editSpecimen").clicked()
        findChild(details, "specimenName").text = "Another unsaved edit"
        mouseClick(navigation)
        confirmation = findChild(details, "discardConfirmation")
        tryCompare(confirmation, "visible", true)
        findChild(confirmation.contentItem, "confirmDiscard").clicked()
        tryCompare(window, "currentSection", 1)
        tryVerify(() => findChild(window, "languageSelector") !== null)
        window.close()
    }
    function test_discard_failed_edit() {
        failOnWarning(/.*/)
        const page = createTemporaryObject(pageComponent, test)
        const plants = findChild(page, "plantModel")
        settle(plants)
        plants.submit(5, plants.selected, "Nishiro", "Gymnocalycium", "Red", "", 0, "2024-02-29")
        settle(plants)
        const grid = findChild(page, "specimenGrid")
        tryCompare(grid, "count", 1)
        grid.itemAtIndex(0).openDetails()
        const details = findChild(page, "specimenDetails")
        settle(findChild(details, "careModel"))
        findChild(details, "editSpecimen").clicked()
        findChild(details, "specimenName").text = "Unsaved name"
        findChild(details, "acquiredOn").text = "2025-02-30"
        findChild(details, "specimenSave").clicked()
        tryCompare(plants, "busy", false)
        verify(plants.error.length > 0)
        verify(findChild(details, "specimenError").visible)
        findChild(details, "discardEdits").clicked()
        compare(details.editing, false)
        compare(findChild(details, "specimenName").text, "Nishiro")
        compare(findChild(details, "acquiredOn").text, "2024-02-29")
        compare(findChild(details, "specimenProfile").currentValue, 0)
        compare(plants.error, "")
        compare(findChild(details, "specimenError").visible, false)
        // A later error from another operation must survive discarding an edit.
        plants.submit(7, "999999", "", "", "", plants.selected)
        tryCompare(plants, "busy", false)
        verify(plants.error.length > 0)
        details.discardEdits()
        verify(plants.error.length > 0)
        findChild(details, "editSpecimen").clicked()
        findChild(details, "acquiredOn").text = "invalid"
        findChild(details, "specimenSave").clicked()
        tryCompare(plants, "busy", false)
        verify(plants.error.length > 0)
        details.discardEdits()
        compare(plants.error, "")
        details.close()
    }
    function test_detail_and_care() {
        failOnWarning(/.*/)
        const page = createTemporaryObject(pageComponent, test)
        verify(page)
        const plants = findChild(page, "plantModel")
        settle(plants)
        plants.submit(1, "", "Care test")
        settle(plants)
        const collection = plants.collections[plants.collections.length - 1].id
        plants.submit(3, collection)
        settle(plants)
        plants.submit(5, collection, "Nishiro", "Gymnocalycium mihanovichii", "Red cap")
        settle(plants)
        const grid = findChild(page, "specimenGrid")
        tryCompare(grid, "count", 1)
        grid.itemAtIndex(0).openDetails()
        let details = findChild(page, "specimenDetails")
        verify(details)
        const id = details.specimenId
        compare(details.editing, false)
        compare(findChild(details, "detailDisplayName").text, "Nishiro")
        compare(findChild(details, "acquisitionDuration").text, App.Ts.get(App.TextId.Specimen_DateUnknown))
        let care = findChild(details, "careModel")
        settle(care)
        compare(care.needs.length, 0)
        compare(care.availableTypes.length, 8)
        compare(care.availableTypes[0].id, "")
        compare(care.availableTypes[0].name, App.Ts.get(App.TextId.Care_Watering))
        compare(care.availableTypes[0].icon.toString(), "qrc:/img/care/watering.svg")
        compare(care.availableTypes[0].actionLabel, App.Ts.get(App.TextId.Care_Watered))
        compare(care.availableTypes[5].actionLabel, "")
        verify(findChild(details, "careEmpty").visible)
        findChild(details, "editSpecimen").clicked()
        findChild(details, "specimenProfile").currentIndex = 1
        findChild(details, "acquiredOn").text = "2024-02-29"
        verify(details.dirty)
        findChild(details, "specimenSave").clicked()
        settle(plants)
        compare(details.editing, false)
        compare(details.specimen.profile, 0)
        compare(details.specimen.acquiredOn, "2024-02-29")
        verify(findChild(details, "acquisitionDuration").text.length > 0)
        findChild(details, "addCareNeed").clicked()
        const selector = findChild(details, "careTypeSelector")
        tryCompare(selector, "visible", true)
        findChild(selector.contentItem, "addNeed_0").clicked()
        compare(care.busy, true)
        compare(care.needs.length, 0) // Queue acceptance is not success.
        settle(care)
        tryCompare(selector, "visible", false)
        compare(care.needs.length, 1)
        verify(care.needs[0].id.length > 0)
        compare(care.needs[0].description, App.Ts.get(App.TextId.Care_WateringInfo))
        compare(care.availableTypes.length, 7)
        verify(care.availableTypes.every(t => t.type !== 0))
        findChild(details, "addCareNeed").clicked()
        findChild(selector.contentItem, "addNeed_1").clicked()
        settle(care)
        compare(care.needs.length, 2)
        const repottingId = care.needs[1].id
        for (const type of [0, 1]) {
            findChild(details, "need_" + type).clicked()
            const needPage = findChild(details, "careNeedDetails")
            verify(needPage.visible)
            compare(needPage.need.type, type)
            keyClick(Qt.Key_Escape)
            compare(details.activeNeed, null)
        }
        // All compact layouts, both themes and enlarged system-style typography scroll vertically.
        for (const dark of [false, true]) {
            MV.Tokens.dark = dark
            for (const scale of [1, 1.5]) {
                MV.Tokens.fontScale = scale
                for (const width of [320, 393, 720]) {
                    page.width = width
                    page.height = 480
                    wait(20)
                    const scroll = findChild(details, "detailsScroll")
                    verify(scroll.contentWidth <= scroll.availableWidth + 1)
                    verify(scroll.contentHeight > scroll.height)
                    verify(grabImage(details).width > 0)
                    const add = findChild(details, "addCareNeed")
                    verify(add.width <= scroll.width)
                    details.editing = true
                    wait(10)
                    tryVerify(() => findChild(details, "specimenName").width <= scroll.width)
                    verify(findChild(details, "specimenProfile").width <= scroll.width)
                    verify(findChild(details, "acquiredOn").width <= scroll.width)
                    details.editing = false
                }
            }
        }
        MV.Tokens.fontScale = 1
        page.width = 960
        page.height = 800
        findChild(details, "need_0").clicked()
        findChild(details, "removeNeed").clicked()
        const confirm = findChild(details, "removeNeedConfirmation")
        tryCompare(confirm, "visible", true)
        findChild(confirm.contentItem, "confirmRemoveNeed").clicked()
        compare(care.needs.length, 2)
        settle(care)
        compare(details.activeNeed, null)
        compare(care.needs.length, 1)
        compare(care.needs[0].id, repottingId)
        verify(care.availableTypes.some(t => t.type === 0))
        details.close()
        tryVerify(() => findChild(page, "specimenDetails") === null)
        grid.itemAtIndex(0).openDetails()
        details = findChild(page, "specimenDetails")
        care = findChild(details, "careModel")
        settle(care)
        compare(care.needs.length, 1)
        compare(care.needs[0].id, repottingId)
        compare(details.specimenId, id)
        compare(details.specimen.profile, 0)
        compare(details.specimen.acquiredOn, "2024-02-29")
        details.close()
        tryVerify(() => findChild(page, "specimenDetails") === null)
    }
}
