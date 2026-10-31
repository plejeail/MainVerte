// clazy:skip
#define DOCTEST_CONFIG_IMPLEMENT
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QTemporaryDir>
#include <QTimer>

#include <doctest.h>

#include "translator.h"
#include "app_settings.h"
#include "widget_wizard_setup.h"


TEST_CASE("wizard language is transient until gardener submission and survives restart") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = (directory.path() + "/test.db").toStdString();
    bool explicit_choice = false;
    SUBCASE("no interaction") {}
    SUBCASE("explicit choice including the displayed language") { explicit_choice = true; }
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        REQUIRE(ts::select_language("fr"));
        SetupWizard wizard(services);
        CHECK(wizard.languageIndex() == 1);
        REQUIRE(wizard.languages().size() == 4);
        if (explicit_choice) {
            wizard.selectLanguage(0);
            CHECK(ts::current_language() == "en");
            wizard.selectLanguage(1);
            CHECK(ts::current_language() == "fr");
            wizard.selectLanguage(1);
        }
        CHECK(mv::settings().language.empty());
        QEventLoop loop;
        bool created = false;
        QObject::connect(&wizard, &SetupWizard::created, &loop, [&](const QString& name) {
            CHECK(name == "Camille");
            created = true;
            loop.quit();
        });
        QObject::connect(&wizard, &SetupWizard::failed, &loop, [&](const QString&) { loop.quit(); });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        wizard.createGardener(" Camille ");
        CHECK(wizard.busy());
        loop.exec();
        REQUIRE(created);
        CHECK_FALSE(wizard.busy());
        CHECK(mv::settings().language == (explicit_choice ? "fr" : ""));
        CHECK(mv::settings().current_gardener_id > 0);
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        CHECK(mv::settings().language == (explicit_choice ? "fr" : ""));
        CHECK(mv::settings().current_gardener_id > 0);
        ts::select_initial_language(mv::settings().language);
        if (explicit_choice) {
            CHECK(ts::current_language() == "fr");
        }
    }
}

TEST_CASE("system language follows UI preferences with English fallback") {
    // Regional formats and preferred UI languages can differ, notably on Windows.
    const auto preferences = QLocale::system().uiLanguages();
    QStringList supported;
    for (const auto code : ts::list_languages()) {
        supported.append(QString::fromUtf8(code.data(), code.size()));
    }
    QString expected = "en";
    for (const auto& preference : preferences) {
        const auto base = preference.section('-', 0, 0);
        if (supported.contains(base)) {
            expected = base;
            break;
        }
    }
    INFO("UI preferences: " << preferences.join(", ").toStdString());
    REQUIRE(ts::select_system_language());
    CHECK(ts::current_language() == expected.toStdString());
}

TEST_CASE("German and Spanish expose translated labels and embedded flags") {
    const auto options = ts::language_options();
    for (const auto* code : {"de", "es"}) {
        ts::select_initial_language(code);
        REQUIRE(ts::current_language() == code);
        const bool german = ts::current_language() == "de";
        CHECK(ts::get(ts::TextId::Settings_LanguageLabel) == (german ? "Sprache" : "Idioma"));
        CHECK(ts::get(ts::TextId::Theme_Dark) == (german ? "Dunkel" : "Oscuro"));
        const auto option = options[ts::language_index()].toMap();
        CHECK(option.value("code").toString() == QString::fromUtf8(code));
        CHECK_FALSE(option.value("label").toString().isEmpty());
        const auto flag = option.value("flag").toString();
        CHECK(QFile::exists(flag.mid(3))); // qrc:/ becomes :/ for QFile.
    }
}

TEST_CASE("saved language takes precedence over the system and invalid settings fall back") {
    ts::select_initial_language("fr");
    CHECK(ts::current_language() == "fr");
    ts::select_initial_language("en");
    CHECK(ts::current_language() == "en");
    ts::select_system_language();
    const auto expected = ts::current_language();
    ts::select_initial_language("unsupported");
    CHECK(ts::current_language() == expected);
    ts::select_initial_language("");
    CHECK(ts::current_language() == expected);
}

TEST_CASE("language activation immediately retranslates QML bindings") {
    mv::Services services;
    REQUIRE(services.initialize(":memory:"));
    REQUIRE(ts::select_language("en"));
    SetupWizard wizard(services);
    QQmlEngine engine;
    QObject::connect(&wizard, &SetupWizard::languageChanged, &engine, &QQmlEngine::retranslate);
    QQmlComponent component(&engine);
    const auto source = QByteArray("import QtQml\nimport WizardTest\nQtObject { property string label: Ts.get(")
        + QByteArray::number(static_cast<int>(ts::TextId::Gardener_LanguageLabel)) + ") }";
    component.setData(source, QUrl());
    REQUIRE_MESSAGE(component.isReady(), component.errorString().toStdString());
    std::unique_ptr<QObject> object(component.create());
    REQUIRE(object);
    CHECK(object->property("label").toString() == "Language");
    wizard.selectLanguage(1);
    CHECK(object->property("label").toString() == "Langue");
    wizard.selectLanguage(0);
    CHECK(object->property("label").toString() == "Language");
}

TEST_CASE("settings language applies immediately and persists without changing the gardener") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = (directory.path() + "/settings.db").toStdString();
    int64_t gardener_id = 0;
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        QEventLoop creation;
        QTimer::singleShot(5000, &creation, &QEventLoop::quit);
        REQUIRE(mv::create_and_select_gardener("Camille", [&](mv::GardenerResult result) {
            REQUIRE(result);
            gardener_id = result->id;
            creation.quit();
        }));
        creation.exec();
        REQUIRE(gardener_id > 0);
        ts::select_language("en");
        AppSettings settings;
        REQUIRE(settings.languages().size() == 4);
        CHECK(settings.languageIndex() == 0);
        CHECK_FALSE(settings.saving());
        CHECK(mv::settings().language.empty());
        settings.selectLanguage(-1);
        settings.selectLanguage(100);
        CHECK_FALSE(settings.saving());
        CHECK(mv::settings().language.empty());

        QQmlEngine engine;
        QObject::connect(&settings, &AppSettings::languageChanged, &engine, &QQmlEngine::retranslate);
        QQmlComponent component(&engine);
        component.setData(QByteArray("import QtQml\nimport WizardTest\nQtObject { property string label: Ts.get(")
            + QByteArray::number(static_cast<int>(ts::TextId::Settings_LanguageLabel)) + ") }", QUrl());
        REQUIRE_MESSAGE(component.isReady(), component.errorString().toStdString());
        std::unique_ptr<QObject> object(component.create());
        REQUIRE(object);
        CHECK(object->property("label").toString() == "Language");

        QEventLoop saving;
        QObject::connect(&settings, &AppSettings::stateChanged, &saving, [&] {
            if (!settings.saving()) saving.quit();
        });
        QTimer::singleShot(5000, &saving, &QEventLoop::quit);
        settings.selectLanguage(1);
        CHECK(settings.saving());
        CHECK(ts::current_language() == "fr");
        CHECK(object->property("label").toString() == "Langue");
        settings.selectLanguage(0); // Ignore further activation while a save is pending.
        CHECK(ts::current_language() == "fr");
        saving.exec();
        REQUIRE_FALSE(settings.saving());
        CHECK_FALSE(settings.saveFailed());
        CHECK(mv::settings().language == "fr");
        CHECK(mv::settings().current_gardener_id == gardener_id);
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        CHECK(mv::settings().language == "fr");
        CHECK(mv::settings().current_gardener_id == gardener_id);
        ts::select_initial_language(mv::settings().language);
        AppSettings settings;
        CHECK(settings.languageIndex() == 1);
    }
}

TEST_CASE("settings page exposes the language selector and retranslates after activation") {
    mv::Services services;
    REQUIRE(services.initialize(":memory:"));
    ts::select_language("en");
    AppSettings settings;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("appSettings", &settings);
    QObject::connect(&settings, &AppSettings::languageChanged, &engine, &QQmlEngine::retranslate);
    QFile source(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../qml/SettingsPage.qml"));
    REQUIRE(source.open(QIODevice::ReadOnly));
    QQmlComponent component(&engine);
    component.setData("import WizardTest\n" + source.readAll(), QUrl());
    if (component.isLoading()) {
        QEventLoop loading;
        QObject::connect(&component, &QQmlComponent::statusChanged, &loading, &QEventLoop::quit);
        QTimer::singleShot(5000, &loading, &QEventLoop::quit);
        loading.exec();
    }
    REQUIRE_MESSAGE(component.isReady(), component.errorString().toStdString());
    std::unique_ptr<QObject> page(component.create());
    REQUIRE(page);
    CHECK(page->property("title").toString() == "Parameters");
    auto* selector = page->findChild<QObject*>("languageSelector");
    REQUIRE(selector);
    CHECK(selector->property("currentIndex").toInt() == 0);
    CHECK(mv::settings().language.empty()); // Opening the page must not create a preference.
    QEventLoop saving;
    QObject::connect(&settings, &AppSettings::stateChanged, &saving, [&] {
        if (!settings.saving()) saving.quit();
    });
    QTimer::singleShot(5000, &saving, &QEventLoop::quit);
    REQUIRE(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 1)));
    CHECK(page->property("title").toString() == "Paramètres");
    saving.exec();
    CHECK_FALSE(settings.saving());
    CHECK_FALSE(settings.saveFailed());
    CHECK(mv::settings().language == "fr");
    auto* theme = page->findChild<QObject*>("themeSelector");
    REQUIRE(theme);
    CHECK(theme->property("currentIndex").toInt() == 0);
    REQUIRE(QMetaObject::invokeMethod(theme, "activated", Q_ARG(int, 1)));
    CHECK(settings.darkTheme());
    CHECK(theme->property("currentIndex").toInt() == 1);
    QTimer::singleShot(5000, &saving, &QEventLoop::quit);
    saving.exec();
    REQUIRE_FALSE(settings.saving());
    CHECK_FALSE(settings.saveFailed());
    CHECK(mv::settings().dark_theme);
    CHECK(mv::settings().language == "fr");
}

TEST_CASE("theme applies immediately and persists across restarts without changing language") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = (directory.path() + "/theme.db").toStdString();
    for (const bool dark : {true, false}) {
        {
            mv::Services services;
            REQUIRE(services.initialize(path));
            AppSettings settings;
            CHECK(settings.darkTheme() == !dark);
            QEventLoop saving;
            QObject::connect(&settings, &AppSettings::stateChanged, &saving, [&] {
                if (!settings.saving()) saving.quit();
            });
            QTimer::singleShot(5000, &saving, &QEventLoop::quit);
            settings.selectTheme(dark);
            CHECK(settings.darkTheme() == dark);
            CHECK(settings.saving());
            settings.selectLanguage(1); // Theme and language share the pending-save guard.
            settings.selectTheme(!dark);
            CHECK(settings.darkTheme() == dark);
            saving.exec();
            REQUIRE_FALSE(settings.saving());
            CHECK_FALSE(settings.saveFailed());
            CHECK(mv::settings().dark_theme == dark);
            CHECK(mv::settings().language.empty());
        }
        {
            mv::Services services;
            REQUIRE(services.initialize(path));
            AppSettings settings;
            CHECK(settings.darkTheme() == dark);
            settings.selectTheme(dark);
            CHECK_FALSE(settings.saving());
            CHECK(mv::settings().language.empty());
        }
    }
}

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    qmlRegisterSingletonType<Ts>("WizardTest", 1, 0, "Ts",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new Ts; });
    qmlRegisterUncreatableMetaObject(mv::i18n::staticMetaObject, "WizardTest", 1, 0, "TextId", "Enum values only");
    doctest::Context context(argc, argv);
    return context.run();
}
