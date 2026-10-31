import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: Ts.get(TextId.Parameters_Title)

    ColumnLayout {
        anchors.left: parent.left
        anchors.margins: 24
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 12

        Label {
            font.bold: true
            text: Ts.get(TextId.Settings_LanguageLabel)
        }

        ComboBox {
            id: languageBox

            Accessible.name: Ts.get(TextId.Settings_LanguageLabel)
            Layout.fillWidth: true
            Layout.maximumWidth: 360
            currentIndex: appSettings.languageIndex
            enabled: !appSettings.saving
            leftPadding: 12
            model: appSettings.languages
            objectName: "languageSelector"
            rightPadding: 32
            textRole: "label"

            contentItem: Row {
                spacing: 12

                Image {
                    fillMode: Image.PreserveAspectFit
                    height: parent.height
                    source: languageBox.currentIndex >= 0 ? languageBox.model[languageBox.currentIndex].flag : ""
                    width: 28
                }

                Text {
                    color: languageBox.palette.buttonText
                    font: languageBox.font
                    height: parent.height
                    text: languageBox.displayText
                    verticalAlignment: Text.AlignVCenter
                }
            }
            delegate: ItemDelegate {
                required property int index
                required property var modelData

                highlighted: languageBox.highlightedIndex === index
                width: languageBox.width

                contentItem: Row {
                    spacing: 12

                    Image {
                        fillMode: Image.PreserveAspectFit
                        height: 24
                        source: modelData.flag
                        width: 28
                    }

                    Text {
                        color: languageBox.palette.text
                        font: languageBox.font
                        text: modelData.label
                    }
                }
            }

            onActivated: index => appSettings.selectLanguage(index)
        }

        Label {
            font.bold: true
            text: Ts.get(TextId.Appearance_Title)
        }

        ComboBox {
            Accessible.name: Ts.get(TextId.Appearance_Title)
            Layout.fillWidth: true
            Layout.maximumWidth: 360
            currentIndex: appSettings.darkTheme ? 1 : 0
            enabled: !appSettings.saving
            model: [Ts.get(TextId.Theme_Light), Ts.get(TextId.Theme_Dark)]
            objectName: "themeSelector"

            onActivated: index => appSettings.selectTheme(index === 1)
        }

        Label {
            Layout.fillWidth: true
            color: "#B3261E"
            text: Ts.get(TextId.Settings_SaveFailed)
            visible: appSettings.saveFailed
            wrapMode: Text.WordWrap
        }

        GroupBox {
            id: debugBox

            Layout.fillWidth: true
            title: "-- DEBUG OPTIONS --"
            visible: true

            Label {
                font.bold: true
                text: "Log level"
            }

            ComboBox {
                Accessible.name: "log level"
                Layout.fillWidth: true
                Layout.maximumWidth: 360
                currentIndex: 0
                enabled: !appSettings.saving
                model: ["Debug", "Info", "Warning", "Error"]
                objectName: "logSelector"

                onActivated: index => appSettings.selectLogLevel(index)
            }
        }
    }
}
