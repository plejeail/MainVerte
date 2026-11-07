// clazy:skip
#define DOCTEST_CONFIG_IMPLEMENT
#include <array>
#include <memory>
#include <string>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariant>
#include <doctest.h>
#include <sqlite3.h>

#include "app_settings.h"
#include "mvlog.h"
#include "translator.h"
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
        wizard.complete_setup(" Camille ");
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
        const auto& option = options[ts::language_index()];
        CHECK(option.code == QString::fromUtf8(code));
        CHECK_FALSE(option.label.isEmpty());
        const auto flag = option.flag;
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
        REQUIRE(mv::complete_setup("Camille", "", [&](mv::GardenerResult result) {
            REQUIRE(result);
            gardener_id = result->id;
            creation.quit();
        }));
        creation.exec();
        REQUIRE(gardener_id > 0);
        ts::select_language("en");
        App_Settings settings;
        REQUIRE(settings.languages().size() == 4);
        CHECK(settings.language_index() == 0);
        CHECK_FALSE(settings.saving());
        CHECK(mv::settings().language.empty());
        settings.select_language(-1);
        settings.select_language(100);
        CHECK_FALSE(settings.saving());
        CHECK(mv::settings().language.empty());

        QQmlEngine engine;
        QObject::connect(&settings, &App_Settings::language_changed, &engine, &QQmlEngine::retranslate);
        QQmlComponent component(&engine);
        component.setData(QByteArray("import QtQml\nimport WizardTest\nQtObject { property string label: Ts.get(")
            + QByteArray::number(static_cast<int>(ts::TextId::Settings_LanguageLabel)) + ") }", QUrl());
        REQUIRE_MESSAGE(component.isReady(), component.errorString().toStdString());
        std::unique_ptr<QObject> object(component.create());
        REQUIRE(object);
        CHECK(object->property("label").toString() == "Language");

        QEventLoop saving;
        QObject::connect(&settings, &App_Settings::state_changed, &saving, [&] {
            if (!settings.saving()) saving.quit();
        });
        QTimer::singleShot(5000, &saving, &QEventLoop::quit);
        settings.select_language(1);
        CHECK(settings.saving());
        CHECK(ts::current_language() == "fr");
        CHECK(object->property("label").toString() == "Langue");
        settings.select_language(0); // Ignore further activation while a save is pending.
        CHECK(ts::current_language() == "fr");
        saving.exec();
        REQUIRE_FALSE(settings.saving());
        CHECK_FALSE(settings.save_failed());
        CHECK(mv::settings().language == "fr");
        CHECK(mv::settings().current_gardener_id == gardener_id);
    }
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        CHECK(mv::settings().language == "fr");
        CHECK(mv::settings().current_gardener_id == gardener_id);
        ts::select_initial_language(mv::settings().language);
        App_Settings settings;
        CHECK(settings.language_index() == 1);
    }
}

TEST_CASE("settings page exposes the language selector and retranslates after activation") {
    mv::Services services;
    REQUIRE(services.initialize(":memory:"));
    ts::select_language("en");
    App_Settings settings;
    QQmlEngine engine;
    QObject::connect(&settings, &App_Settings::language_changed, &engine, &QQmlEngine::retranslate);
    QFile source(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../qml/SettingsPage.qml"));
    REQUIRE(source.open(QIODevice::ReadOnly));
    QQmlComponent component(&engine);
    component.setData(source.readAll().replace("import QtQuick\r\n", "import QtQuick\n")
                          .replace("import QtQuick\n", "import WizardTest\nimport QtQuick\n"), QUrl());
    if (component.isLoading()) {
        QEventLoop loading;
        QObject::connect(&component, &QQmlComponent::statusChanged, &loading, &QEventLoop::quit);
        QTimer::singleShot(5000, &loading, &QEventLoop::quit);
        loading.exec();
    }
    REQUIRE_MESSAGE(component.isReady(), component.errorString().toStdString());
    std::unique_ptr<QObject> page(component.createWithInitialProperties({{"settings", QVariant::fromValue(&settings)}}));
    REQUIRE(page);
    CHECK(page->property("title").toString() == "Parameters");
    auto* selector = page->findChild<QObject*>("languageSelector");
    REQUIRE(selector);
    CHECK(selector->property("currentIndex").toInt() == 0);
    CHECK(selector->property("count").toInt() == 4);
    CHECK(selector->property("currentText").toString() == settings.languages().front().label);
    QQmlExpression optionFields(engine.rootContext(), page.get(),
        "settings.languages[0].code + '|' + settings.languages[0].flag");
    CHECK(optionFields.evaluate().toString() == "en|qrc:/img/flag_en.svg");
    CHECK_FALSE(optionFields.hasError());
    CHECK(mv::settings().language.empty()); // Opening the page must not create a preference.
    QEventLoop saving;
    QObject::connect(&settings, &App_Settings::state_changed, &saving, [&] {
        if (!settings.saving()) saving.quit();
    });
    QTimer::singleShot(5000, &saving, &QEventLoop::quit);
    REQUIRE(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 1)));
    CHECK(page->property("title").toString() == "Paramètres");
    saving.exec();
    CHECK_FALSE(settings.saving());
    CHECK_FALSE(settings.save_failed());
    CHECK(mv::settings().language == "fr");
    auto* theme = page->findChild<QObject*>("themeSelector");
    REQUIRE(theme);
    CHECK(theme->property("currentIndex").toInt() == 0);
    REQUIRE(QMetaObject::invokeMethod(theme, "activated", Q_ARG(int, 1)));
    CHECK(settings.dark_theme());
    CHECK(theme->property("currentIndex").toInt() == 1);
    QTimer::singleShot(5000, &saving, &QEventLoop::quit);
    saving.exec();
    REQUIRE_FALSE(settings.saving());
    CHECK_FALSE(settings.save_failed());
    CHECK(mv::settings().dark_theme_enabled);
    CHECK(mv::settings().language == "fr");
}

TEST_CASE("setup page uses its injected wizard for language selection and errors") {
    mv::Services services;
    REQUIRE(services.initialize(":memory:"));
    ts::select_language("en");
    SetupWizard wizard(services);
    QQmlEngine engine;
    QObject::connect(&wizard, &SetupWizard::languageChanged, &engine, &QQmlEngine::retranslate);
    QFile source(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../qml/Page_wizard_setup.qml"));
    REQUIRE(source.open(QIODevice::ReadOnly));
    QQmlComponent component(&engine);
    component.setData(source.readAll().replace("import QtQuick\r\n", "import QtQuick\n")
                          .replace("import QtQuick\n", "import WizardTest\nimport QtQuick\n"), QUrl());
    if (component.isLoading()) {
        QEventLoop loading;
        QObject::connect(&component, &QQmlComponent::statusChanged, &loading, &QEventLoop::quit);
        QTimer::singleShot(5000, &loading, &QEventLoop::quit);
        loading.exec();
    }

    REQUIRE_MESSAGE(component.isReady(), component.errorString().toStdString());
    std::unique_ptr<QObject> page(component.createWithInitialProperties({{"wizard", QVariant::fromValue(&wizard)}}));
    REQUIRE(page);
    auto* selector = page->findChild<QObject*>("languageSelector");
    REQUIRE(selector);
    CHECK(selector->property("currentIndex").toInt() == 0);
    CHECK(selector->property("count").toInt() == 4);
    CHECK(selector->property("currentText").toString() == wizard.languages().front().label);
    REQUIRE(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 1)));
    CHECK(wizard.languageIndex() == 1);
    CHECK(selector->property("currentIndex").toInt() == 1);
    CHECK(mv::settings().language.empty());
    wizard.complete_setup("");
    CHECK(page->property("errorMessage").toString() == ts::get(ts::TextId::Gardener_NameRequired));
    auto* name = page->findChild<QObject*>("gardenerName");
    REQUIRE(name);
    REQUIRE(name->setProperty("text", "Camille"));
    QEventLoop saving;
    bool created = false;
    QObject::connect(&wizard, &SetupWizard::created, &saving, [&](const QString&) {
        created = true;
        saving.quit();
    });
    QTimer::singleShot(5000, &saving, &QEventLoop::quit);
    REQUIRE(QMetaObject::invokeMethod(page.get(), "submit"));
    CHECK(wizard.busy());
    saving.exec();
    REQUIRE(created);
    CHECK(page->property("errorMessage").toString().isEmpty());
    CHECK(mv::settings().language == "fr");
    CHECK(mv::settings().current_gardener_id > 0);
    CHECK(mv::settings().current_collection_id > 0);
}

TEST_CASE("theme applies immediately and persists across restarts without changing language") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = (directory.path() + "/theme.db").toStdString();
    for (const bool dark : {true, false}) {
        {
            mv::Services services;
            REQUIRE(services.initialize(path));
            App_Settings settings;
            CHECK(settings.dark_theme() == !dark);
            QEventLoop saving;
            QObject::connect(&settings, &App_Settings::state_changed, &saving, [&] {
                if (!settings.saving()) saving.quit();
            });
            QTimer::singleShot(5000, &saving, &QEventLoop::quit);
            settings.select_theme(dark);
            CHECK(settings.dark_theme() == dark);
            CHECK(settings.saving());
            settings.select_language(1); // Theme and language share the pending-save guard.
            settings.select_theme(!dark);
            CHECK(settings.dark_theme() == dark);
            saving.exec();
            REQUIRE_FALSE(settings.saving());
            CHECK_FALSE(settings.save_failed());
            CHECK(mv::settings().dark_theme_enabled == dark);
            CHECK(mv::settings().language.empty());
        }
        {
            mv::Services services;
            REQUIRE(services.initialize(path));
            App_Settings settings;
            CHECK(settings.dark_theme() == dark);
            settings.select_theme(dark);
            CHECK_FALSE(settings.saving());
            CHECK(mv::settings().language.empty());
        }
    }
}

TEST_CASE("debug visibility unlocks after five version clicks and persists both states") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = (directory.path() + "/debug.db").toStdString();
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        App_Settings settings;
        CHECK_FALSE(settings.debug_enabled());
        CHECK(settings.software_version() == "0.1.0");
        CHECK(settings.databaseVersion() == mv::database_version());
        QQmlEngine engine;
        QFile source(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../qml/SettingsPage.qml"));
        REQUIRE(source.open(QIODevice::ReadOnly));
        QQmlComponent component(&engine);
        component.setData(source.readAll().replace("import QtQuick\r\n", "import QtQuick\n")
                              .replace("import QtQuick\n", "import WizardTest\nimport QtQuick\n"), QUrl());
        if (component.isLoading()) {
            QEventLoop loading;
            QObject::connect(&component, &QQmlComponent::statusChanged, &loading, &QEventLoop::quit);
            QTimer::singleShot(5000, &loading, &QEventLoop::quit);
            loading.exec();
        }

        REQUIRE_MESSAGE(component.isReady(), component.errorString().toStdString());
        std::unique_ptr<QObject> page(component.createWithInitialProperties({{"settings", QVariant::fromValue(&settings)}}));
        REQUIRE(page);
        auto* debug = page->findChild<QObject*>("debugBox");
        REQUIRE(debug);
        CHECK_FALSE(debug->property("visible").toBool());
        for (int click = 0; click < 4; ++click) {
            REQUIRE(QMetaObject::invokeMethod(page.get(), "versionClicked"));
            CHECK_FALSE(settings.debug_enabled());
        }

        QEventLoop saving;
        QObject::connect(&settings, &App_Settings::state_changed, &saving, [&] {
            if (!settings.saving()) {
                saving.quit();
            }
        });
        QTimer::singleShot(5000, &saving, &QEventLoop::quit);
        REQUIRE(QMetaObject::invokeMethod(page.get(), "versionClicked"));
        CHECK(settings.debug_enabled());
        CHECK(debug->property("visible").toBool());
        settings.select_debug_enabled(false); // Ignore changes while the write is pending.
        CHECK(settings.debug_enabled());
        saving.exec();
        REQUIRE_FALSE(settings.saving());
        REQUIRE_FALSE(settings.save_failed());
        CHECK(mv::settings().debug_enabled);
    }

    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        App_Settings settings;
        REQUIRE(settings.debug_enabled());
        QEventLoop saving;
        QObject::connect(&settings, &App_Settings::state_changed, &saving, [&] {
            if (!settings.saving()) {
                saving.quit();
            }
        });
        QTimer::singleShot(5000, &saving, &QEventLoop::quit);
        settings.select_debug_enabled(false);
        saving.exec();
        REQUIRE_FALSE(settings.saving());
        REQUIRE_FALSE(settings.save_failed());
    }

    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        App_Settings settings;
        CHECK_FALSE(settings.debug_enabled());
    }
}

TEST_CASE("failed language writes retain the saved preference across restart and allow retry") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = (directory.path() + "/language-failure.db").toStdString();
    std::string previous_preference;
    SUBCASE("automatic preference") {}
    SUBCASE("explicit preference") { previous_preference = "fr"; }
    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        auto snapshot = mv::settings();
        snapshot.language = previous_preference;
        QEventLoop saving;
        bool saved = false;
        REQUIRE(mv::save_settings(snapshot, [&](bool success) {
            saved = success;
            saving.quit();
        }));
        QTimer::singleShot(5000, &saving, &QEventLoop::quit);
        saving.exec();
        REQUIRE(saved);
    }

    // Change the fixture only while Services is stopped: SQLite is single-threaded.
    {
        sqlite3* raw = nullptr;
        const int opened = sqlite3_open(path.c_str(), &raw);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
        REQUIRE(opened == SQLITE_OK);
        REQUIRE(sqlite3_exec(database.get(),
            "CREATE TRIGGER reject_language BEFORE UPDATE ON settings BEGIN "
            "SELECT RAISE(ABORT, 'language write rejected'); END;",
            nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    ts::select_initial_language(previous_preference);
    const std::string previous_effective(ts::current_language());
    const int previous_index = ts::language_index();
    const int requested_index = (previous_index + 1) % static_cast<int>(ts::Lang_Count);
    const std::string requested_language(ts::list_languages()[requested_index]);
    for (int attempt = 0; attempt < 2; ++attempt) {
        mv::Services services;
        REQUIRE(services.initialize(path));
        CHECK(mv::settings().language == previous_preference);
        App_Settings settings;
        QEventLoop saving;
        QObject::connect(&settings, &App_Settings::state_changed, &saving, [&] {
            if (!settings.saving()) {
                saving.quit();
            }
        });
        QTimer::singleShot(5000, &saving, &QEventLoop::quit);
        settings.select_language(requested_index);
        CHECK(settings.saving());
        CHECK(ts::current_language() == requested_language);
        settings.select_language(previous_index);
        saving.exec();
        REQUIRE_FALSE(settings.saving());
        CHECK(settings.save_failed());
        CHECK(settings.language_index() == previous_index);
        CHECK(ts::current_language() == previous_effective);
        CHECK(mv::settings().language == previous_preference);
    }

    {
        sqlite3* raw = nullptr;
        const int opened = sqlite3_open(path.c_str(), &raw);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
        REQUIRE(opened == SQLITE_OK);
        REQUIRE(sqlite3_exec(database.get(), "DROP TRIGGER reject_language;",
            nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        CHECK(mv::settings().language == previous_preference);
        App_Settings settings;
        QEventLoop saving;
        QObject::connect(&settings, &App_Settings::state_changed, &saving, [&] {
            if (!settings.saving()) {
                saving.quit();
            }
        });
        QTimer::singleShot(5000, &saving, &QEventLoop::quit);
        settings.select_language(requested_index);
        saving.exec();
        REQUIRE_FALSE(settings.saving());
        CHECK_FALSE(settings.save_failed());
        CHECK(mv::settings().language == requested_language);
        CHECK(ts::current_language() == requested_language);
    }

    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        CHECK(mv::settings().language == requested_language);
    }
}

TEST_CASE("log threshold and selector follow saved settings across navigation and restart") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = (directory.path() + "/log-level.db").toStdString();
    int previous = 0;
    const std::array levels{mvlog::Level::Debug, mvlog::Level::Info, mvlog::Level::Warning, mvlog::Level::Error};
    for (const int index : {1, 3, 2, 0}) {
        mv::Services services;
        REQUIRE(services.initialize(path));
        CHECK(static_cast<int>(mv::settings().log_level) == previous);
        mvlog::set_level(mvlog::Level::Error);
        App_Settings settings;
        CHECK(settings.log_level() == previous);
        CHECK(mvlog::level() == levels[previous]);
        QEventLoop saving;
        QObject::connect(&settings, &App_Settings::state_changed, &saving, [&] {
            if (!settings.saving()) {
                saving.quit();
            }
        });
        QTimer::singleShot(5000, &saving, &QEventLoop::quit);
        settings.select_log_level(index);
        CHECK(settings.saving());
        CHECK(settings.log_level() == index);
        CHECK(mvlog::level() == levels[index]);
        settings.select_log_level(previous);
        saving.exec();
        REQUIRE_FALSE(settings.saving());
        CHECK_FALSE(settings.save_failed());
        CHECK(static_cast<int>(mv::settings().log_level) == index);
        QQmlEngine engine;
        QFile source(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../qml/SettingsPage.qml"));
        REQUIRE(source.open(QIODevice::ReadOnly));
        QQmlComponent component(&engine);
        component.setData(source.readAll().replace("import QtQuick\r\n", "import QtQuick\n")
            .replace("import QtQuick\n", "import WizardTest\nimport QtQuick\n"), QUrl());
        if (component.isLoading()) {
            QEventLoop loading;
            QObject::connect(&component, &QQmlComponent::statusChanged, &loading, &QEventLoop::quit);
            QTimer::singleShot(5000, &loading, &QEventLoop::quit);
            loading.exec();
        }

        REQUIRE_MESSAGE(component.isReady(), component.errorString().toStdString());
        for (int visit = 0; visit < 2; ++visit) {
            std::unique_ptr<QObject> page(component.createWithInitialProperties({{"settings", QVariant::fromValue(&settings)}}));
            REQUIRE(page);
            auto* selector = page->findChild<QObject*>("logSelector");
            REQUIRE(selector);
            CHECK(selector->property("currentIndex").toInt() == index);
        }

        previous = index;
    }

    // Inject a real SQLite failure with Core stopped, then exercise the app callback.
    {
        sqlite3* raw = nullptr;
        const int opened = sqlite3_open(path.c_str(), &raw);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, &sqlite3_close);
        REQUIRE(opened == SQLITE_OK);
        REQUIRE(sqlite3_exec(database.get(),
            "CREATE TRIGGER reject_log_level BEFORE UPDATE ON settings BEGIN "
            "SELECT RAISE(ABORT, 'log level rejected'); END;", nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        App_Settings settings;
        CHECK(settings.log_level() == 0);
        QEventLoop saving;
        QObject::connect(&settings, &App_Settings::state_changed, &saving, [&] {
            if (!settings.saving()) {
                saving.quit();
            }
        });
        QTimer::singleShot(5000, &saving, &QEventLoop::quit);
        settings.select_log_level(3);
        saving.exec();
        REQUIRE_FALSE(settings.saving());
        CHECK(settings.save_failed());
        CHECK(settings.log_level() == 0);
        CHECK(mvlog::level() == mvlog::Level::Debug);
        CHECK(mv::settings().log_level == mv::LogLevel::Dbg);
    }

    {
        mv::Services services;
        REQUIRE(services.initialize(path));
        CHECK(mv::settings().log_level == mv::LogLevel::Dbg);
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
