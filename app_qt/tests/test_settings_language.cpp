// clazy:skip
#define DOCTEST_CONFIG_IMPLEMENT
#include <deque>
#include <memory>
#include <string>
#include <utility>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTimer>
#include <QVariant>
#include <doctest.h>

#include "app_settings.h"
#include "mv_core.h"
#include "mvlog/mvlog.h"
#include "translator.h"


namespace {

// Exercise the app against Core's completion contract, including queue refusal,
// which cannot occur while the real Services worker is running normally.
struct PendingSave {
    mv::Settings settings;
    mv::MoveOnlyFunction<void(bool)> callback;
    bool accepted;
};

mv::Settings persisted;
std::deque<PendingSave> pending;
bool accept_submission = true;

void complete_save(bool success) {
    REQUIRE(pending.size() == 1);
    auto job = std::move(pending.front());
    pending.pop_front();
    const bool saved = job.accepted && success;
    if (saved) {
        persisted = job.settings;
    }

    job.callback(saved);
}

}  //  namespace

mv::Settings mv::settings() {
    return persisted;
}

int mv::database_version() {
    return 0;
}

bool mv::save_settings(const Settings& settings, MoveOnlyFunction<void(bool)> callback) {
    pending.push_back({settings, std::move(callback), accept_submission});
    return accept_submission;
}

TEST_CASE("language completion restores the interface and preserves automatic preferences") {
    persisted = {};
    pending.clear();
    accept_submission = true;
    bool success = false;
    SUBCASE("successful persistence") { success = true; }
    SUBCASE("accepted write fails") {}
    SUBCASE("submission refused") { accept_submission = false; }
    SUBCASE("explicit preference write fails") { persisted.language = "fr"; }
    ts::select_initial_language(persisted.language);
    const auto previous_preference = persisted.language;
    const std::string previous_effective(ts::current_language());
    const int previous_index = ts::language_index();
    const int requested_index = (previous_index + 1) % static_cast<int>(ts::Lang_Count);
    const std::string requested_language(ts::list_languages()[requested_index]);
    AppSettings settings;
    QQmlEngine engine;
    QObject::connect(&settings, &AppSettings::languageChanged, &engine, &QQmlEngine::retranslate);
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
    auto* selector = page->findChild<QObject*>("languageSelector");
    REQUIRE(selector);
    CHECK(selector->property("currentIndex").toInt() == previous_index);
    const auto previous_title = page->property("title").toString();
    int language_changes = 0;
    QObject::connect(&settings, &AppSettings::languageChanged, &settings, [&] {
        ++language_changes;
        if (language_changes == 2) {
            // Rollback notification must not admit a new write before it finishes.
            settings.selectLanguage(requested_index);
        }
    });

    REQUIRE(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, requested_index)));
    CHECK(settings.saving());
    CHECK_FALSE(settings.saveFailed());
    CHECK(ts::current_language() == requested_language);
    CHECK(selector->property("currentIndex").toInt() == requested_index);
    CHECK(page->property("title").toString() == ts::get(ts::TextId::Parameters_Title));
    CHECK(page->property("title").toString() != previous_title);
    CHECK_FALSE(selector->property("enabled").toBool());
    CHECK(persisted.language == previous_preference);
    REQUIRE(pending.size() == 1);
    CHECK(pending.front().settings.language == requested_language);
    settings.selectLanguage(previous_index);
    settings.selectLanguage((requested_index + 1) % static_cast<int>(ts::Lang_Count));
    REQUIRE(pending.size() == 1);
    CHECK(ts::current_language() == requested_language);
    complete_save(success);

    CHECK_FALSE(settings.saving());
    CHECK(settings.saveFailed() == !success);
    CHECK(selector->property("enabled").toBool());
    CHECK(pending.empty());
    CHECK(language_changes == (success ? 1 : 2));
    CHECK(persisted.language == (success ? requested_language : previous_preference));
    CHECK(ts::current_language() == (success ? requested_language : previous_effective));
    CHECK(selector->property("currentIndex").toInt() == (success ? requested_index : previous_index));
    CHECK(page->property("title").toString() == ts::get(ts::TextId::Parameters_Title));
    if (!success) {
        CHECK(page->property("title").toString() == previous_title);
        bool visible_error = false;
        for (const auto* child : page->findChildren<QObject*>()) {
            if (child->property("text").toString() == ts::get(ts::TextId::Settings_SaveFailed)
                && child->property("visible").toBool()) {
                visible_error = true;
            }
        }

        CHECK(visible_error);
    }

    // A later accepted change clears the error and settles in submission order.
    accept_submission = true;
    settings.selectLanguage(previous_index);
    CHECK(settings.saving());
    CHECK_FALSE(settings.saveFailed());
    settings.selectLanguage(requested_index);
    REQUIRE(pending.size() == 1);
    complete_save(true);
    CHECK_FALSE(settings.saving());
    CHECK_FALSE(settings.saveFailed());
    CHECK(persisted.language == previous_effective);
    CHECK(ts::current_language() == previous_effective);
    CHECK(selector->property("currentIndex").toInt() == previous_index);
}

TEST_CASE("log level completion restores the threshold and QML selector on failure") {
    persisted = {};
    persisted.log_level = mv::LogLevel::Wrn;
    pending.clear();
    accept_submission = true;
    bool success = false;
    SUBCASE("success") { success = true; }
    SUBCASE("database failure") {}
    SUBCASE("submission refusal") { accept_submission = false; }
    AppSettings settings;
    CHECK(settings.logLevel() == 2);
    CHECK(mvlog::level() == mvlog::Level::Warning);
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
    auto* selector = page->findChild<QObject*>("logSelector");
    REQUIRE(selector);
    CHECK(selector->property("currentIndex").toInt() == 2);
    for (const int invalid : {-1, 4, 256}) {
        settings.selectLogLevel(invalid);
        CHECK_FALSE(settings.saving());
        CHECK(pending.empty());
        CHECK(settings.logLevel() == 2);
        CHECK(mvlog::level() == mvlog::Level::Warning);
    }

    int changes = 0;
    QObject::connect(&settings, &AppSettings::logLevelChanged, &settings, [&] {
        ++changes;
        if (changes == 2) {
            settings.selectLogLevel(1); // Ignore reentrant changes during rollback.
        }
    });
    REQUIRE(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 3)));
    CHECK(settings.saving());
    CHECK(settings.logLevel() == 3);
    CHECK(selector->property("currentIndex").toInt() == 3);
    CHECK(mvlog::level() == mvlog::Level::Error);
    settings.selectLogLevel(0);
    REQUIRE(pending.size() == 1);
    CHECK(persisted.log_level == mv::LogLevel::Wrn);
    complete_save(success);
    CHECK_FALSE(settings.saving());
    CHECK(settings.saveFailed() == !success);
    CHECK(settings.logLevel() == (success ? 3 : 2));
    CHECK(selector->property("currentIndex").toInt() == (success ? 3 : 2));
    CHECK(mvlog::level() == (success ? mvlog::Level::Error : mvlog::Level::Warning));
    CHECK(persisted.log_level == (success ? mv::LogLevel::Err : mv::LogLevel::Wrn));
    CHECK(pending.empty());
    CHECK(changes == (success ? 1 : 2));
}

TEST_CASE("log level failure restores the effective threshold even if the settings object is gone") {
    persisted = {};
    pending.clear();
    accept_submission = true;
    auto settings = std::make_unique<AppSettings>();
    mvlog::set_level(mvlog::Level::Info);
    settings->selectLogLevel(3);
    CHECK(mvlog::level() == mvlog::Level::Error);
    settings.reset();
    complete_save(false);
    CHECK(mvlog::level() == mvlog::Level::Info);
    CHECK(persisted.log_level == mv::LogLevel::Dbg);
    mvlog::set_level(mvlog::Level::Debug);
}

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    qmlRegisterSingletonType<Ts>("WizardTest", 1, 0, "Ts",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new Ts; });
    qmlRegisterUncreatableMetaObject(mv::i18n::staticMetaObject, "WizardTest", 1, 0, "TextId", "Enum values only");
    doctest::Context context(argc, argv);
    return context.run();
}
