import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MainVerteStyle as MV

ColumnLayout {
    property string value
    property string caption
    property color measurementColor: MV.Tokens.text
    spacing: MV.Tokens.space1
    Label { text: parent.value; font: MV.Tokens.headingFont; color: parent.measurementColor }
    Label { text: parent.caption; font: MV.Tokens.captionFont; color: MV.Tokens.textSecondary }
}
