#pragma once

#include <QString>
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

#include "generated_translations.hpp"

namespace ts {
using TextId = mv::i18n::TextId;
static constexpr auto Lang_Count = mv::i18n::languages.size();

std::array<std::string_view, Lang_Count> list_languages();

// not thread-wise
bool select_language(std::string_view lang);
bool select_system_language();
std::string_view current_language();
[[nodiscard]] QString get(TextId id);
}  // namespace ts

class Ts : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Ts(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE [[nodiscard]] QString get(int id) const;
};
