#include "translator.h"

#include <cstdint>

#include <QLocale>
#include <QQmlEngine>
#include <QString>

#include "generated_translations.hpp"


namespace {
constexpr ts::TextId First_Id = static_cast<ts::TextId>(0);
constexpr int Text_Count = mv::i18n::languages[0].texts.size();
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

bool ts::select_language(std::string_view lang) {
    for (const auto& candidate : mv::i18n::languages) {
        if (candidate.code == lang) {
            qInfo() << "ts selected language" << candidate.code;
            selected_language = candidate;
            return true;
        }
    }

    return false;
}

bool ts::select_system_language() {
    const auto preferences = QLocale::system().uiLanguages(QLocale::TagSeparator::Underscore);

    for (const QString& language : preferences) {
        const QByteArray code = language.toUtf8();
        std::string_view code_str{code.constData(), static_cast<std::size_t>(code.size())};
        if (ts::select_language(code_str)) {
            return true;
        }
    }

    return ts::select_language("en");
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
