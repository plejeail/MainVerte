# MainVerte design system

MainVerte uses a quiet botanical identity: ivory and mineral surfaces, forest
green actions, terracotta supporting accents, and the platform's own typeface.
The light palette is canonical. Dark mode uses charcoal botanical surfaces and
softer sage actions, with independently chosen semantic colors.

## Architecture and installation

`app_qt/style/` is the **MainVerteStyle** QML module, embedded as a static QML
plugin. `main.cpp` imports the plugin and calls `QQuickStyle::setStyle()` before
creating either QML engine. CMake links the style plugin and its resources into
the application on every platform; no loose style files or environment variables
are required for deployment. Qt's Basic style is the explicit fallback.
The style declares its 6.12 module version explicitly. Basic is selected through
`setFallbackStyle()`, rather than re-exporting its controls from the style module.

Controls derive from their corresponding `QtQuick.Controls.Basic` implementation.
They remain Qt Quick Controls backed by Qt Quick Templates, with Qt's input,
accessibility, action, editing, popup, model, implicit sizing and RTL behavior.
The style replaces visual delegates and metrics rather than recreating that
behavior. `ActionContent` uses Qt's `QtQuick.Controls.impl.IconLabel` for icon,
text, mnemonic and display-mode layout, as the built-in styles do; recheck this
implementation-module dependency when upgrading Qt.

Small visual helpers share action/input surfaces, selection indicators, switches,
and slider geometry. There are no image assets, font downloads, shaders, blur
effects, additional runtime libraries, or C++ theme services. Surfaces use tonal
separation and a single border instead of shadows.

## Tokens and theme selection

Import the module with an alias to avoid shadowing `QtQuick.Controls` names:

```qml
import QtQuick.Controls
import MainVerteStyle as MV

Button {
    text: qsTr("Water plant")
    highlighted: true
    onClicked: MV.Tokens.dark = !MV.Tokens.dark
}
```

`MV.Tokens.dark` defaults to `false`. Changing it updates existing controls,
including open popups, through live semantic palette bindings. The normal app's
Appearance settings and the separate showcase both expose this choice. The
application persists the theme in Core settings and restores it at startup.
The showcase choice remains session-local and never opens the journal database. The application explicitly chooses its style, so
`QT_QUICK_CONTROLS_STYLE` is not used to override it.

`Tokens.qml` is the single source of truth:

| Family | Meaning |
| --- | --- |
| `background`, `surface`, `surfaceSecondary`, `elevated` | Window, grouped content, quieter regions, inputs/popups |
| `text`, `textSecondary`, `textTertiary`, `disabledText` | Content hierarchy and unavailable content |
| `accent`, `accentHover`, `accentPressed`, `textOnAccent` | Primary and checked actions |
| `hover`, `pressed`, `selection`, `disabledSurface` | Neutral interactions and selection |
| `border`, `separator`, `focus` | Essential boundaries, decorative division, keyboard focus |
| `success`, `warning`, `error`, `information` | Status, always accompanied by a text label |
| `healthy`, `moisture`, `sunlight`, `soil` | Contextual plant information |
| `displayFont` through `supportingFont` | System font, 7 semantic type roles |
| `space1` … `space7` | 4, 8, 12, 16, 24, 32, 48 logical-pixel spacing |
| `radiusSmall`, `radius`, `radiusLarge` | 4, 8, 12 logical-pixel corner radii |

Typography uses point sizes, with a base at least as large as the application's
system font. The scale is display 2.5×, heading 1.7×, title 1.25×, body/label 1×,
caption/supporting 0.9×. Labels use demi-bold weight. Multiline body labels use
1.4× proportional line height; large headings use 1.2×. `fontScale` is an
additional application override (default 1), not a replacement for system scaling.

Geometry is expressed in Qt logical pixels. Desktop controls have a 36-pixel
minimum; Android/iOS controls have a 48-pixel minimum. Text can increase heights.
Sliders and small visual indicators retain a full control hit area. Interactive
page and selection handles use at least 48 pixels on touch platforms. Layouts
change at `compactBreakpoint` (720); the app changes between side and bottom
navigation with available width. Mobile safe-area insets are retained by the
window and drawer. There are no screen-resolution or DPI assumptions.

Busy/progress controls retain their native accessible roles and should always
have adjacent status text. Transitions use the `motionDuration` token.

## Component coverage

| Family | Styled controls |
| --- | --- |
| Actions | Button, RoundButton, DelayButton, ToolButton |
| Selection | CheckBox, RadioButton, Switch, CheckDelegate, RadioDelegate, SwitchDelegate |
| Delegates and menus | ItemDelegate, SwipeDelegate, MenuItem, MenuBarItem, MenuSeparator |
| Editing | TextField, TextArea, SpinBox, **DoubleSpinBox** (6.11), ComboBox, **SearchField** (6.10) |
| Numeric selection | Slider, RangeSlider, Dial, Tumbler |
| Navigation | TabBar, TabButton, ToolBar, MenuBar, Menu, Drawer, SwipeView, PageIndicator, SplitView, StackView |
| Containers | ApplicationWindow, Page, Pane, Frame, GroupBox, ScrollView, Popup, Dialog, DialogButtonBox, ToolTip |
| Feedback | ProgressBar, BusyIndicator, ScrollBar, ScrollIndicator, ToolSeparator, Label |
| Models | TableViewDelegate, TreeViewDelegate, HorizontalHeaderViewDelegate, VerticalHeaderViewDelegate, HorizontalHeaderView, VerticalHeaderView, SelectionRectangle |
| Calendar | DayOfWeekRow, MonthGrid, WeekNumberColumn |
| Primitives | AbstractButton, Control, Container |

The 6.12 Basic style inventory was inspected. Nonvisual **Action, ActionGroup,
ButtonGroup, Calendar and CalendarModel** deliberately use Basic fallback: they
have no visual delegates to replace. SwipeView's page mechanics and ScrollView's
flick/scroll mechanics are retained. Header views and calendar primitives keep
Qt's model-sensitive delegates with the shared font/palette; table/header/tree
cell surfaces are customized separately. There are no copies of Qt interaction
implementations.

Default, hover, press, focus, disabled, checked and highlighted states are handled
where supported by each Qt type. TextField, TextArea, ComboBox, SpinBox and
DoubleSpinBox add an explicit `invalid` property; set it only when validation
should be shown. Use the explicit style type (for example `MV.TextField`) when
declaring this additional property, so QML tooling can resolve it statically.
`acceptableInput` may also be false for legitimate intermediate
editing, so it does not automatically turn a field red. Provide a persistent
label and a textual error with `Accessible.description`. Do not communicate
status by color alone.

## Showcase

Build `appMainVerte` normally and run it with `--showcase`:

```powershell
.\build\desktop\app_qt\Debug\appMainVerte.exe --showcase
```

For a single-config build the executable is normally under `build/desktop/app_qt/`.
Qt Creator can pass `--showcase` in Run Settings, including for an Android kit.
Without the flag, normal application startup and Core initialization are retained.
The gallery does not initialize Core or open the journal database.

The separate **MainVerte.Showcase** module includes typography, colors, actions,
editing, selection/settings, navigation, containers, dialogs/popups, feedback,
table/calendar examples, plant summary, care reminder, and measurements. It has
desktop and compact layouts, light/dark switching. Its form
uses transient demo state, checks required input, and never writes journal data.

## Extending and validating

Use semantic tokens in application layouts. Prefer standard controls imported
from `QtQuick.Controls`; do not import a platform style in production pages.
To change a component, edit its file under `style/`, retaining the matching Basic
base. Add new QML files explicitly to the style module's CMake list. Use existing
helpers for visual state and keep business rules in Core. New token colors need
both light and dark values plus contrast validation.

With `BUILD_TESTING=ON`, Qt Quick Test is required for the `mainverte_style_tests`
target. After building, run:

```powershell
ctest --test-dir build/desktop -C Debug --output-on-failure --no-tests=error
cmake --build build/desktop --config Debug --target mainverte_style_qmllint mainverte_showcase_qmllint appMainVerte_qmllint
```

Omit configuration arguments for a single-config build. CTest's style test uses
the offscreen platform and software renderer; use a real Windows/Android kit for
platform deployment and actual pointer, touch, screen reader and high-DPI checks.
Automated tests validate style discovery, light/dark creation, existing-control
theme updates, keyboard/selection/input behavior, form validation, desktop/phone
gallery layouts, and 150% type scaling. Contrast tests enforce 4.5:1 for text and
semantic colors on the four surface layers, and 3:1 for essential borders/focus.
Disabled content is exempt from WCAG's contrast minimum. High-contrast preference
also widens focus rings and restores quiet control boundaries.

Qt 6.12's PageIndicator and Tumbler instantiate delegates using mutable contexts.
Those two files intentionally retain unbound delegate contexts: adding
`pragma ComponentBehavior: Bound` produces Qt runtime warnings. `qmllint` therefore
reports its unqualified-context suggestions for these references; they are not
suppressed. Other style, showcase, and application QML is linted normally.

## Validation performed

Validated with Qt 6.12.0, GCC 14.2, a fresh Debug Ninja configuration under
`build/style-linux`, the offscreen platform, and Qt Quick's software renderer:

- Built `appMainVerte`, `mainverte_style_tests`, and `mainverte_core_tests`.
- All three CTest suites passed: Core, translation generator, and style.
- Style tests exercised 23 cases including both themes, registered style
  discovery, existing-control palettes/text, model delegates, keyboard input,
  selection, contrast, form submission, phone/desktop layouts, 150% text, and a
  480-pixel-high scrolling dialog. Runtime warnings fail the affected test.
- Regression tests load the production main window and resize it repeatedly
  across the navigation breakpoint, checking that its requested size is retained.
  Progress tests check actual opacity changes, full-opacity restoration, and
  restart after determinate mode and visibility transitions.
- Normal application and `--showcase` startup smoke checks produced no QML
  warnings. The normal startup check used an isolated temporary data directory.
- Inspected rendered light/dark, input/selection, navigation, compact and enlarged
  text views. The complete Qt 6.12 Basic inventory contains 64 customized controls
  plus the five documented nonvisual fallbacks.

Native Windows/Android builds, Android ABIs and device touch/input methods,
screen readers, native high-DPI rendering, and GPU backends remain unverified in
this Linux environment. The installed Qt kit also reports optional configuration
diagnostics for TaskTree, Vulkan headers and GLX; the software build and tests
succeeded without changing project warnings or dependencies to hide them.
