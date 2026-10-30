import QtQuick
import QtQuick.Controls

Page {
    id: window

    property string errorMessage: ""

    function prepareLayout() {
        formContent.forceLayout();
        form.forceLayout();
    }

    function submit() {
        if (setupWizard.busy || nameField.text.trim().length === 0)
            return;
        window.errorMessage = "";
        setupWizard.createGardener(nameField.text);
    }

    implicitHeight: form.implicitHeight + 96
    title: Ts.get(TextId.App_Title)

    Connections {
        function onFailed(message) {
            window.errorMessage = message;
        }

        target: setupWizard
    }

    Rectangle {
        anchors.fill: parent

        gradient: Gradient {
            orientation: Gradient.Vertical

            GradientStop {
                color: "#143F2C"
                position: 0.0
            }

            GradientStop {
                color: "#0C3427"
                position: 0.55
            }

            GradientStop {
                color: "#071F19"
                position: 1.0
            }
        }
    }

    // Oversized, low-contrast leaf-like arcs add texture without obscuring text.
    Item {
        anchors.fill: parent
        clip: true

        Rectangle {
            border.color: "#295E40"
            border.width: 2
            color: "transparent"
            height: width * 1.35
            opacity: 0.55
            radius: width / 2
            rotation: -28
            width: Math.max(360, parent.width * 0.85)
            x: parent.width - width * 0.55
            y: -height * 0.35
        }

        Rectangle {
            border.color: "#347B52"
            border.width: 2
            color: "transparent"
            height: width * 1.4
            opacity: 0.35
            radius: width / 2
            rotation: 31
            width: Math.max(320, parent.width * 0.78)
            x: -width * 0.57
            y: parent.height - height * 0.48
        }
    }

    Flickable {
        id: scroll

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: Math.max(height, form.implicitHeight + 96)
        contentWidth: width

        Column {
            id: form

            spacing: 22
            width: Math.min(500, Math.max(0, scroll.width - 40))
            x: (scroll.width - width) / 2
            y: (scroll.contentHeight - implicitHeight) / 2

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                border.color: "#638E6B"
                border.width: 1
                color: "#275C43"
                height: 68
                radius: 22
                width: 68

                Rectangle {
                    anchors.centerIn: parent
                    color: "#BEE8AA"
                    height: 34
                    radius: 18
                    rotation: 42
                    width: 22
                }

                Rectangle {
                    color: "#D5F4C6"
                    height: 24
                    radius: 2
                    rotation: -38
                    width: 3
                    x: 28
                    y: 32
                }
            }

            Text {
                color: "#BFE9B5"
                font.bold: true
                font.letterSpacing: 5
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                text: "MAINVERTE"
                width: parent.width
            }

            Text {
                color: "#F2FAE8"
                font.pixelSize: Math.min(40, Math.max(29, window.width * 0.065))
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.08
                text: Ts.get(TextId.Gardener_Slogan)
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Text {
                color: "#B8D6BE"
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.35
                text: Ts.get(TextId.Gardener_Profile)
                width: parent.width
                wrapMode: Text.WordWrap
            }

            Rectangle {
                border.color: "#477B5B"
                border.width: 1
                color: "#174635"
                height: formContent.implicitHeight + 52
                radius: 26
                width: parent.width

                Column {
                    id: formContent

                    spacing: 16
                    width: parent.width - 52
                    x: 26
                    y: 26

                    Text {
                        color: "#D4EFD1"
                        font.bold: true
                        font.letterSpacing: 1.2
                        font.pixelSize: 12
                        text: Ts.get(TextId.Gardener_NameLabel)
                        width: parent.width
                    }

                    TextField {
                        id: nameField

                        Accessible.name: Ts.get(TextId.Gardener_NameLabel)
                        color: "#F2FFE9"
                        enabled: !setupWizard.busy
                        font.pixelSize: 17
                        height: 58
                        leftPadding: 18
                        maximumLength: 30
                        placeholderText: Ts.get(TextId.Gardener_NamePlaceholder)
                        placeholderTextColor: "#A4CBAA"
                        rightPadding: 18
                        selectedTextColor: "#102D20"
                        selectionColor: "#79B88D"
                        width: parent.width

                        background: Rectangle {
                            border.color: nameField.activeFocus ? "#D2F6BB" : "#92CBA0"
                            border.width: nameField.activeFocus ? 2 : 1
                            color: "#0D382B"
                            radius: 12
                        }

                        onAccepted: window.submit()
                        onTextChanged: window.errorMessage = ""
                    }

                    Text {
                        color: "#D4EFD1"
                        font.bold: true
                        font.pixelSize: 12
                        text: Ts.get(TextId.Gardener_LanguageLabel)
                    }

                    ComboBox {
                        id: languageBox

                        Accessible.name: Ts.get(TextId.Gardener_LanguageLabel)
                        model: setupWizard.languages
                        textRole: "label"
                        currentIndex: setupWizard.languageIndex
                        enabled: !setupWizard.busy
                        width: parent.width
                        height: 58
                        leftPadding: 14
                        rightPadding: 32

                        background: Rectangle {
                            color: "#0D382B"
                            border.color: languageBox.activeFocus ? "#D2F6BB" : "#92CBA0"
                            radius: 12
                        }
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
                                color: "#F2FFE9"
                                font.pixelSize: 17
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
                                    color: "#F2FFE9"
                                    font.pixelSize: 17
                                }
                            }
                            background: Rectangle {
                                color: parent.highlighted ? "#477B5B" : "#0D382B"
                            }
                        }
                        onActivated: index => {
                            window.errorMessage = "";
                            setupWizard.selectLanguage(index);
                        }
                    }

                    Text {
                        color: "#FFD3C7"
                        font.pixelSize: 14
                        text: window.errorMessage
                        visible: window.errorMessage.length > 0
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        id: submitButton

                        enabled: (!setupWizard.busy && nameField.text.trim().length > 0)
                        height: 56
                        width: parent.width

                        background: Rectangle {
                            border.color: submitButton.enabled ? "#E0FFD2" : "#85A590"
                            border.width: 1
                            color: submitButton.enabled ? "#C4EDB1" : "#85A590"
                            radius: 13
                        }
                        contentItem: Text {
                            color: submitButton.enabled ? "#123825" : "#688773"
                            font.bold: true
                            font.pixelSize: 16
                            horizontalAlignment: Text.AlignHCenter
                            text: setupWizard.busy ? Ts.get(TextId.Gardener_Busy) : Ts.get(TextId.Gardener_Create)
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: window.submit()
                    }
                }
            }

            Text {
                color: "#86B89A"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                text: Ts.get(TextId.Gardener_Privacy)
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
    }
}
