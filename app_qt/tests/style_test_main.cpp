#include <QQuickStyle>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuickTest/quicktest.h>


Q_IMPORT_QML_PLUGIN(MainVerteStylePlugin)
Q_IMPORT_QML_PLUGIN(MainVerteShowcasePlugin)

int main(int argc, char** argv) {
    QQuickStyle::setStyle(QStringLiteral("MainVerteStyle"));
    QQuickStyle::setFallbackStyle(QStringLiteral("Basic"));
    return quick_test_main(argc, argv, "mainverte_style", QUICK_TEST_SOURCE_DIR);
}
