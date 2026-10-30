import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MainVerteStyle as MV

Pane {
    id: section
    property string title
    property string caption
    default property alias entries: entries.data
    padding: MV.Tokens.space5
    contentItem: ColumnLayout {
        spacing: MV.Tokens.space4
        Label { text: section.title; font: MV.Tokens.titleFont; Layout.fillWidth: true; wrapMode: Text.Wrap }
        Label {
            text: section.caption
            visible: text.length > 0
            color: MV.Tokens.textSecondary
            font: MV.Tokens.captionFont
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        ColumnLayout { id: entries; spacing: MV.Tokens.space3; Layout.fillWidth: true }
    }
}
