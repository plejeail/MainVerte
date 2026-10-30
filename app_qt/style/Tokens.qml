pragma Singleton
import QtQuick

QtObject {
    // Light is the canonical identity. Every color has a perceptually chosen dark partner.
    property bool dark: false
    property real fontScale: 1
    readonly property bool touch: Qt.platform.os === "android" || Qt.platform.os === "ios"
    readonly property bool highContrast: Application.styleHints.accessibility.contrastPreference === Qt.HighContrast
    readonly property color background: palette.window
    readonly property color surface: palette.base
    readonly property color surfaceSecondary: palette.alternateBase
    readonly property color elevated: palette.toolTipBase
    readonly property color text: palette.text
    readonly property color textSecondary: dark ? "#c0cbbf" : "#526354"
    readonly property color textTertiary: palette.placeholderText
    readonly property color disabledText: palette.disabled.text
    readonly property color border: palette.mid
    readonly property color separator: palette.midlight
    readonly property color accent: palette.highlight
    readonly property color accentHover: dark ? "#c3dab9" : "#284c35"
    readonly property color accentPressed: dark ? "#9ab88e" : "#203e2c"
    readonly property color textOnAccent: palette.brightText
    readonly property color secondaryAccent: palette.linkVisited
    readonly property color hover: palette.light
    readonly property color pressed: dark ? "#41533f" : "#d4dfcc"
    readonly property color selection: dark ? "#3c513d" : "#dfe9d7"
    readonly property color disabledSurface: palette.disabled.base
    readonly property color focus: dark ? "#e7c38e" : "#805426"
    readonly property color success: dark ? "#b2d6a6" : "#365f3b"
    readonly property color warning: dark ? "#e8c88c" : "#80581f"
    readonly property color error: dark ? "#f0b4a7" : "#a0382b"
    readonly property color information: dark ? "#aecedc" : "#386476"
    readonly property color moisture: information
    readonly property color sunlight: warning
    readonly property color soil: secondaryAccent
    readonly property color healthy: success
    readonly property color scrim: palette.shadow

    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 24
    readonly property int space6: 32
    readonly property int space7: 48
    readonly property int margin: touch ? space4 : space5
    readonly property int controlHeight: Math.max(touch ? 48 : 36, Math.ceil(basePointSize * 2.8))
    readonly property int targetSize: touch ? 48 : 28
    readonly property int indicatorSize: 20
    readonly property int iconSize: 20
    readonly property int trackSize: 4
    readonly property int handleSize: touch ? 28 : 22
    readonly property int radiusSmall: 4
    readonly property int radius: 8
    readonly property int radiusLarge: 12
    readonly property int hairline: 1
    readonly property int focusWidth: highContrast ? 3 : 2
    readonly property int motionDuration: 140
    readonly property int contentWidth: 1120
    readonly property int compactBreakpoint: 720
    readonly property real lineHeight: 1.4
    readonly property real headingLineHeight: 1.2
    readonly property real basePointSize: Math.max(touch ? 12 : 11, Application.font.pointSize) * fontScale
    readonly property string fontFamily: Application.font.family
    readonly property font displayFont: Qt.font({family: fontFamily, pointSize: basePointSize * 2.5, weight: Font.DemiBold})
    readonly property font headingFont: Qt.font({family: fontFamily, pointSize: basePointSize * 1.7, weight: Font.DemiBold})
    readonly property font titleFont: Qt.font({family: fontFamily, pointSize: basePointSize * 1.25, weight: Font.DemiBold})
    readonly property font bodyFont: Qt.font({family: fontFamily, pointSize: basePointSize, weight: Font.Normal})
    readonly property font labelFont: Qt.font({family: fontFamily, pointSize: basePointSize, weight: Font.DemiBold})
    readonly property font captionFont: Qt.font({family: fontFamily, pointSize: basePointSize * 0.9, weight: Font.Normal})
    readonly property font supportingFont: captionFont

    // Controls copy palettes. Switching objects reapplies every role to existing controls.
    readonly property Palette palette: dark ? darkPalette : lightPalette
    readonly property Palette lightPalette: Palette {
        window: "#f5f4ee"
        windowText: "#23392d"
        base: "#fcfcf7"
        alternateBase: "#ecefe5"
        text: "#23392d"
        button: "#ecefe5"
        buttonText: "#23392d"
        brightText: "#ffffff"
        highlight: "#345c42"
        highlightedText: "#ffffff"
        placeholderText: "#60705e"
        light: "#e2e9db"
        midlight: "#d4dccd"
        mid: "#7b8d76"
        dark: "#345c42"
        shadow: "#66203025"
        toolTipBase: "#ffffff"
        toolTipText: "#23392d"
        link: "#345c42"
        linkVisited: "#92543d"
        accent: "#345c42"
        disabled {
            windowText: "#7e8a7a"
            text: "#7e8a7a"
            buttonText: "#7e8a7a"
            brightText: "#7e8a7a"
            highlightedText: "#7e8a7a"
            placeholderText: "#7e8a7a"
            dark: "#7e8a7a"
            base: "#e9ece4"
            button: "#e9ece4"
            highlight: "#e9ece4"
        }
    }
    readonly property Palette darkPalette: Palette {
        window: "#18201c"
        windowText: "#eef2e9"
        base: "#212b25"
        alternateBase: "#2a352e"
        text: "#eef2e9"
        button: "#2a352e"
        buttonText: "#eef2e9"
        brightText: "#1c3021"
        highlight: "#b2cda8"
        highlightedText: "#1c3021"
        placeholderText: "#adbbaa"
        light: "#354439"
        midlight: "#445246"
        mid: "#879882"
        dark: "#b2cda8"
        shadow: "#a0101812"
        toolTipBase: "#303d34"
        toolTipText: "#eef2e9"
        link: "#b2cda8"
        linkVisited: "#e1b49e"
        accent: "#b2cda8"
        disabled {
            windowText: "#839182"
            text: "#839182"
            buttonText: "#839182"
            brightText: "#839182"
            highlightedText: "#839182"
            placeholderText: "#839182"
            dark: "#839182"
            base: "#2a322c"
            button: "#2a322c"
            highlight: "#2a322c"
        }
    }
}
