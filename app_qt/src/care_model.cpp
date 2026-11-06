#include "care_model.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include <QPointer>
#include <QUrl>

#include "translator.h"

namespace {

using T = ts::TextId;
struct NeedPresentation {
    mv::NeedType type;
    T name;
    T description;
    const char* icon;
    std::optional<T> action;
};
// One presentation catalogue, including future action labels. No actions are executed.
constexpr std::array catalogue{
    NeedPresentation{mv::NeedType::Watering, T::Care_Watering, T::Care_WateringInfo, "watering", T::Care_Watered},
    NeedPresentation{mv::NeedType::Repotting, T::Care_Repotting, T::Care_RepottingInfo, "repotting", T::Care_Repotted},
    NeedPresentation{mv::NeedType::Fertilizing, T::Care_Fertilizing, T::Care_FertilizingInfo, "fertilizing", T::Care_Fertilized},
    NeedPresentation{mv::NeedType::Pruning, T::Care_Pruning, T::Care_PruningInfo, "pruning", T::Care_Pruned},
    NeedPresentation{mv::NeedType::Treatment, T::Care_Treatment, T::Care_TreatmentInfo, "treatment", T::Care_Treated},
    NeedPresentation{mv::NeedType::Light, T::Care_Light, T::Care_LightInfo, "light", std::nullopt},
    NeedPresentation{mv::NeedType::Humidity, T::Care_Humidity, T::Care_HumidityInfo, "humidity", std::nullopt},
    NeedPresentation{mv::NeedType::Temperature, T::Care_Temperature, T::Care_TemperatureInfo, "temperature", std::nullopt}
};
QVariantMap presentation(const NeedPresentation& entry) {
    return {{QStringLiteral("type"), static_cast<int>(entry.type)}, {QStringLiteral("name"), ts::get(entry.name)},
            {QStringLiteral("description"), ts::get(entry.description)},
            {QStringLiteral("icon"), QUrl(QStringLiteral("qrc:/img/care/%1.svg").arg(QString::fromLatin1(entry.icon)))},
            {QStringLiteral("actionLabel"), entry.action ? ts::get(*entry.action) : QString{}}};
}

}  //  namespace

void CareModel::setSpecimenId(const QString& id) {
    if (id == _specimenId) return;

    _specimenId = id;
    _needs.clear();
    _loaded = false;
    _error.clear();
    emit changed();
    // An in-flight completion will refresh the newly selected specimen.
    if (!_busy) refresh();
}

QVariantList CareModel::needs() const {
    QVariantList result;
    for (const auto& need : _needs) {
        const auto entry = std::ranges::find(catalogue, need.type, &NeedPresentation::type);
        if (entry == catalogue.end()) continue;

        auto value = presentation(*entry);
        value.insert(QStringLiteral("id"), QString::number(need.id));
        result.append(value);
    }

    return result;
}

QVariantList CareModel::availableTypes() const {
    QVariantList result;
    if (!_loaded) return result;

    for (const auto& entry : catalogue) {
        if (std::ranges::none_of(_needs, [&](const auto& need) { return need.type == entry.type; })) {
            result.append(presentation(entry));
        }
    }

    return result;
}

void CareModel::refresh() { submit({.specimen_id = _specimenId.toLongLong()}); }
void CareModel::add(int type) {
    // Duplicate filtering is a UI convention, not a Core invariant.
    if (!_loaded || std::ranges::any_of(_needs, [type](const auto& need) { return static_cast<int>(need.type) == type; })) return;

    submit({.kind = mv::CareAction::Kind::Add, .specimen_id = _specimenId.toLongLong(), .type = static_cast<mv::NeedType>(type)});
}
void CareModel::remove(const QString& id) {
    submit({.kind = mv::CareAction::Kind::Delete, .specimen_id = _specimenId.toLongLong(), .id = id.toLongLong()});
}
void CareModel::submit(mv::CareAction action) {
    if (_busy || _specimenId.isEmpty()) return;

    _busy = true;
    _error.clear();
    emit changed();
    const QPointer<CareModel> receiver(this);
    const auto specimen = _specimenId;
    mv::manage_care(action, [receiver, specimen, kind = action.kind](mv::CareResult result) {
        if (!receiver) return;

        receiver->_busy = false;
        if (specimen != receiver->_specimenId) {
            receiver->refresh();
            return;
        }

        if (result) {
            receiver->_needs = std::move(*result);
            receiver->_loaded = true;
        } else {
            receiver->_error = ts::get(ts::TextId::Care_Failed);
        }

        emit receiver->changed();
        if (result && kind != mv::CareAction::Kind::List) emit receiver->saved();
    });
}
