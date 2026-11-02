// clazy:skip
#include <QEventLoop>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuickTest/quicktest.h>

#include "mv_core.h"

Q_IMPORT_QML_PLUGIN(MainVerteStylePlugin)
Q_IMPORT_QML_PLUGIN(MainVerteShowcasePlugin)

class StyleTestSetup : public QObject {
    Q_OBJECT
public:
    StyleTestSetup() {
        QQuickStyle::setStyle(QStringLiteral("MainVerteStyle"));
        QQuickStyle::setFallbackStyle(QStringLiteral("Basic"));
    }
public slots:
    void applicationAvailable() {
        if (!services.initialize((directory.path() + "/style.db").toStdString())) qFatal("Test database initialization failed");
        QEventLoop loop;
        bool created = false;
        mv::complete_setup("Test gardener", "", [&](mv::GardenerResult result) {
            created = result.has_value();
            loop.quit();
        }, "My Plants");
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        if (!created) qFatal("Test gardener creation failed");
    }
private:
    QTemporaryDir directory;
    mv::Services services;
};

QUICK_TEST_MAIN_WITH_SETUP(mainverte_style, StyleTestSetup)
#include "style_test_main.moc"
