#include "translator.h"

#include <cstdint>

#include <QLocale>
#include <QQmlEngine>
#include <QString>
#include <QVariantMap>

#include "generated_translations.hpp"
#include "mvlog/mvlog.h"


namespace {
constexpr ts::TextId First_Id = static_cast<ts::TextId>(0);
constexpr int Text_Count = static_cast<int>(ts::TextId::Count);
mv::i18n::Language selected_language = mv::i18n::languages[0];
}

std::array<std::string_view, ts::Lang_Count> ts::list_languages() {
    static_assert(ts::Lang_Count == mv::i18n::languages.size());

    std::array<std::string_view, ts::Lang_Count> res{};
    for (uint32_t i=0; i < mv::i18n::languages.size(); ++i) {
        res[i] = mv::i18n::languages[i].code;
    }

    return res;
}

QVariantList ts::language_options() {
    QVariantList result;
    for (const auto code : ts::list_languages()) {
        const auto language = QString::fromUtf8(code.data(), code.size());
        const QLocale locale(language);
        const QString resource = QStringLiteral("qrc:/img/flag_%1.svg").arg(language);
        result.append(
            QVariantMap{
                {"code", language},
                {"label", locale.nativeLanguageName()},
                {"flag", resource}
            }
        );
    }

    return result;
}

int ts::language_index() {
    const auto codes = ts::list_languages();
    for (std::size_t i = 0; i < codes.size(); ++i) {
        if (codes[i] == ts::current_language()) {
            return static_cast<int>(i);
        }
    }

    return 0;
}

bool ts::select_language(std::string_view lang) {
    for (const auto& candidate : mv::i18n::languages) {
        if (candidate.code == lang) {
            mvlog::inf("ts selected language {}", candidate.code);
            selected_language = candidate;
            return true;
        }
    }

    return false;
}

bool ts::select_system_language() {
    const auto preferences = QLocale::system().uiLanguages(QLocale::TagSeparator::Underscore);

    for (const QString& language : preferences) {
        const QByteArray code = language.section('_', 0, 0).toUtf8();
        std::string_view code_str{code.constData(), static_cast<std::size_t>(code.size())};
        if (ts::select_language(code_str)) {
            return true;
        }
    }

    return ts::select_language("en");
}

void ts::select_initial_language(std::string_view saved_language) {
    if (!saved_language.empty()) {
        if (select_language(saved_language)) {
            return;
        }

        mvlog::wrn("failed to select registered language {}", saved_language);
    }

    select_system_language();
}

std::string_view ts::current_language() {
    return selected_language.code;
}

QString ts::get(ts::TextId id) {
    assert(id >= First_Id && id < ts::TextId::Count);
    if (id < static_cast<ts::TextId>(0) || id >= ts::TextId::Count) {
        return {};
    }

    return selected_language.texts[static_cast<uint32_t>(id)];
}

QString Ts::get(int id) const {
    assert(id >= 0 && id < Text_Count);
    if (id < 0 || id >= Text_Count) {
        return {};
    }

    if (auto* engine = qmlEngine(this)) {
        engine->markCurrentFunctionAsTranslationBinding();
    }

    return ts::get(static_cast<ts::TextId>(id));
}
