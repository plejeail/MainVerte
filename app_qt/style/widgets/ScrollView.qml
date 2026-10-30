import QtQuick
import QtQuick.Controls.Basic as Basic


Basic.ScrollView {
    id: root
    palette: Tokens.palette
    font: Tokens.bodyFont

    Basic.ScrollBar.vertical: ScrollBar {
        parent: root
        x: root.mirrored ? 0 : root.width - width
        y: root.topPadding
        height: root.availableHeight
        active: root.Basic.ScrollBar.horizontal.active
    }
    Basic.ScrollBar.horizontal: ScrollBar {
        parent: root
        x: root.leftPadding
        y: root.height - height
        width: root.availableWidth
        active: root.Basic.ScrollBar.vertical.active
    }

}
