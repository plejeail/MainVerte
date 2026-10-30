import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: Ts.get(TextId.Parameters_Title)

    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        spacing: 12

        Label {
            text: Ts.get(TextId.Settings_LanguageLabel)
            font.bold: true
        }

        ComboBox {
            id: languageBox
            objectName: "languageSelector"

            Layout.fillWidth: true
            Layout.maximumWidth: 360
            Accessible.name: Ts.get(TextId.Settings_LanguageLabel)
            model: appSettings.languages
            textRole: "label"
            currentIndex: appSettings.languageIndex
            enabled: !appSettings.saving
            leftPadding: 12
            rightPadding: 32

            contentItem: Row {
                spacing: 12
                Image {
                    source: languageBox.currentIndex >= 0 ? languageBox.model[languageBox.currentIndex].flag : ""
                    width: 28
                    height: parent.height
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    text: languageBox.displayText
                    color: languageBox.palette.buttonText
                    font: languageBox.font
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                }
            }
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                width: languageBox.width
                highlighted: languageBox.highlightedIndex === index
                contentItem: Row {
                    spacing: 12
                    Image {
                        source: modelData.flag
                        width: 28
                        height: 24
                        fillMode: Image.PreserveAspectFit
                    }
                    Text {
                        text: modelData.label
                        font: languageBox.font
                        color: languageBox.palette.text
                    }
                }
            }
            onActivated: index => appSettings.selectLanguage(index)
        }

        Label {
            text: Ts.get(TextId.Appearance_Title)
            font.bold: true
        }

        ComboBox {
            objectName: "themeSelector"
            Layout.fillWidth: true
            Layout.maximumWidth: 360
            Accessible.name: Ts.get(TextId.Appearance_Title)
            model: [Ts.get(TextId.Theme_Light), Ts.get(TextId.Theme_Dark)]
            currentIndex: appSettings.darkTheme ? 1 : 0
            enabled: !appSettings.saving
            onActivated: index => appSettings.selectTheme(index === 1)
        }

        Label {
            Layout.fillWidth: true
            text: Ts.get(TextId.Settings_SaveFailed)
            visible: appSettings.saveFailed
            wrapMode: Text.WordWrap
            color: "#B3261E"
        }
    }
}
