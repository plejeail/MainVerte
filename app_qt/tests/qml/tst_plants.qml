import QtQuick
import QtQuick.Controls
import QtTest
import MainVerte as App
import MainVerteStyle as MV

TestCase {
    id: test
    name: "Plants"
    width: 1280
    height: 900
    visible: true
    when: windowShown
    Component { id: pageComponent; App.CollectionPage { width: test.width; height: test.height } }
    function cleanup() { MV.Tokens.dark = false; MV.Tokens.fontScale = 1 }
    function settle(model) { tryCompare(model, "busy", false); compare(model.error, "") }
    function test_workflow() {
        const page = createTemporaryObject(pageComponent, test)
        verify(page)
        const model = findChild(page, "plantModel")
        const grid = findChild(page, "specimenGrid")
        const selector = findChild(page, "collectionSelector")
        verify(model)
        settle(model)
        compare(grid.count, 0)
        const original = model.selected
        model.submit(4, original)
        tryCompare(model, "busy", false)
        compare(model.error, App.Ts.get(App.TextId.Plants_LastCollection))
        model.refresh()
        settle(model)
        compare(model.collections.length, 1)
        compare(model.selected, original)
        model.submit(5, original, "Nishiro", "Gymnocalycium mihanovichii", "")
        compare(model.busy, true)
        settle(model)
        tryCompare(grid, "count", 1)
        model.submit(3, "999999")
        compare(grid.count, 1)
        tryCompare(model, "busy", false)
        verify(model.error.length > 0)
        compare(model.selected, original)
        compare(selector.currentValue, original)
        compare(grid.count, 1)
        model.refresh()
        settle(model)
        for (const width of [320, 480, 960, 1280]) {
            page.width = width
            wait(20)
            verify(grid.cellWidth <= grid.width)
            verify(grid.cellWidth > 0)
            verify(grid.contentWidth <= grid.width + 1)
        }
        page.width = 960
        wait(20)
        const card = grid.itemAtIndex(0)
        verify(card)
        mouseClick(card, card.width / 2, card.height / 2)
        const details = findChild(page, "specimenDetails")
        tryCompare(details, "visible", true)
        compare(findChild(page, "specimenName").text, "Nishiro")
        page.openedId = "999999"
        findChild(page, "specimenName").text = "Unsaved name"
        mouseClick(findChild(page, "specimenSave"))
        tryCompare(model, "busy", false)
        verify(model.error.length > 0)
        compare(details.visible, true)
        compare(findChild(page, "specimenName").text, "Unsaved name")
        details.close()
        grid.forceActiveFocus()
        keyClick(Qt.Key_Return)
        tryCompare(details, "visible", true)
        compare(findChild(page, "specimenName").text, "Nishiro")
        details.close()
        model.submit(1, "", "Cuttings")
        settle(model)
        compare(model.collections.length, 2)
        const second = model.collections[1].id
        model.submit(3, second)
        compare(grid.count, 1) // Previous confirmed collection stays visible until completion.
        settle(model)
        compare(model.selected, second)
        compare(grid.count, 0)
        model.submit(5, second, " ")
        tryCompare(model, "busy", false)
        verify(model.error.length > 0)
        compare(grid.count, 0)
        model.refresh()
        settle(model)
        model.submit(3, original)
        settle(model)
        compare(grid.count, 1)
        compare(selector.currentValue, original)
        const id = grid.itemAtIndex(0).plantId
        model.submit(8, id)
        settle(model)
        compare(grid.count, 0)
        model.showArchived(true)
        compare(grid.count, 1)
        model.submit(9, id)
        settle(model)
        compare(grid.count, 0)
        model.showArchived(false)
        compare(grid.count, 1)
        wait(30)
        verify(grabImage(page).width > 0)
        // Management actions never discard dirty fields, and no-op moves are disabled.
        grid.itemAtIndex(0).openDetails()
        tryCompare(details, "visible", true)
        const target = findChild(page, "moveDestination")
        const move = findChild(page, "moveSpecimen")
        compare(target.currentValue, original)
        compare(move.enabled, false)
        findChild(page, "specimenName").text = "Unsaved edit"
        compare(page.dirty, true)
        compare(move.enabled, false)
        compare(target.enabled, false)
        findChild(page, "discardEdits").clicked()
        compare(findChild(page, "specimenName").text, "Nishiro")
        compare(page.dirty, false)
        target.currentIndex = target.indexOfValue(second)
        verify(move.enabled)
        page.openedId = "999999"
        move.clicked()
        tryCompare(model, "busy", false)
        verify(model.error.length > 0)
        compare(details.visible, true)
        compare(findChild(page, "specimenName").text, "Nishiro")
        page.openedId = id
        move.clicked()
        settle(model)
        tryCompare(details, "visible", false)
        compare(grid.count, 0)
        model.submit(3, second)
        settle(model)
        compare(grid.count, 1)
        // Twenty real records; delegates remain virtualized.
        for (let i = 0; i < 19; ++i) {
            model.submit(5, second, "A very long specimen name that should wrap and elide ".repeat(3) + i, "Hidden botanical metadata", "Hidden cultivar")
            settle(model)
        }
        compare(grid.count, 20)
        const sizes = [[320,480],[360,640],[393,852],[480,800],[960,700],[1280,800]]
        for (const dark of [false, true]) {
            MV.Tokens.dark = dark
            for (const scale of [1, 1.5]) {
                MV.Tokens.fontScale = scale
                for (const size of sizes) {
                    page.width = size[0]
                    page.height = size[1]
                    wait(20)
                    verify(grid.columns >= 2)
                    if (size[0] < MV.Tokens.compactBreakpoint) compare(grid.columns, 2)
                    if (size[0] >= MV.Tokens.compactBreakpoint) compare(grid.columns, Math.max(2, Math.floor(grid.width / 220)))
                    verify(grid.cellWidth <= grid.width / 2)
                    verify(grid.cellHeight < grid.cellWidth * 1.4)
                    const card = grid.itemAtIndex(0)
                    verify(card)
                    const label = findChild(card, "cardName")
                    compare(label.maximumLineCount, 2)
                    compare(label.lineCount, 2)
                    verify(label.truncated)
                    verify(label.width <= card.width)
                    verify(label.contentHeight <= label.height + 1)
                    const labels = findChild(card, "specimenCard").contentItem.children
                    compare(labels.length, 2) // Illustration and name only.
                    const count = findChild(page, "activeCount")
                    const archived = findChild(page, "archivedToggle")
                    verify(archived.x >= count.x + count.width)
                    const add = findChild(page, "addPlant")
                    verify(grid.mapToItem(page, 0, grid.height).y <= add.mapToItem(page, 0, 0).y)
                    verify(add.mapToItem(page, 0, add.height).y <= page.height)
                    verify(grid.contentWidth <= grid.width + 1)
                }
            }
        }
    }
}
