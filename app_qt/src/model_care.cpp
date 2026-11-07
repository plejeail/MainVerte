#include "model_care.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include <QPointer>
#include <QUrl>

#include "translator.h"
#include "mv_core.h"

namespace {

struct Need_Presentation {
    mv::NeedType type;
    ts::TextId name;
    ts::TextId description;
    const char* icon;
    std::optional<ts::TextId> action;
};

// One presentation catalogue, including future action labels. No actions are executed.
constexpr std::array catalogue{
    Need_Presentation{
        .type = mv::NeedType::Watering,
        .name = ts::TextId::Care_Watering,
        .description = ts::TextId::Care_WateringInfo,
        .icon = "watering",
        .action = ts::TextId::Care_Watered
    },
    Need_Presentation{
        .type = mv::NeedType::Repotting,
        .name = ts::TextId::Care_Repotting,
        .description = ts::TextId::Care_RepottingInfo,
        .icon = "repotting",
        .action = ts::TextId::Care_Repotted
    },
    Need_Presentation{
        .type = mv::NeedType::Fertilizing,
        .name = ts::TextId::Care_Fertilizing,
        .description = ts::TextId::Care_FertilizingInfo,
        .icon = "fertilizing",
        .action = ts::TextId::Care_Fertilized
    },
    Need_Presentation{
        .type = mv::NeedType::Pruning,
        .name = ts::TextId::Care_Pruning,
        .description = ts::TextId::Care_PruningInfo,
        .icon = "pruning",
        .action = ts::TextId::Care_Pruned
    },
    Need_Presentation{
        .type = mv::NeedType::Treatment,
        .name = ts::TextId::Care_Treatment,
        .description = ts::TextId::Care_TreatmentInfo,
        .icon = "treatment",
        .action = ts::TextId::Care_Treated
    },
    Need_Presentation{
        .type = mv::NeedType::Light,
        .name = ts::TextId::Care_Light,
        .description = ts::TextId::Care_LightInfo,
        .icon = "light",
        .action = std::nullopt
    },
    Need_Presentation{
        .type = mv::NeedType::Humidity,
        .name = ts::TextId::Care_Humidity,
        .description = ts::TextId::Care_HumidityInfo,
        .icon = "humidity",
        .action = std::nullopt
    },
    Need_Presentation{
        .type = mv::NeedType::Temperature,
        .name = ts::TextId::Care_Temperature,
        .description = ts::TextId::Care_TemperatureInfo,
        .icon = "temperature",
        .action = std::nullopt
    }
};

CareNeedOption presentation(const Need_Presentation& entry) {
    return {
        .id   = {},
        .type = static_cast<int>(entry.type),
        .name = ts::get(entry.name),
        .description = ts::get(entry.description),
        .icon = QUrl(QStringLiteral("qrc:/img/care/%1.svg").arg(QString::fromLatin1(entry.icon))),
        .action_label = entry.action ? ts::get(*entry.action) : QString{}
    };
}

}  //  namespace

void Care_Model::set_specimen_id(const QString& id) {
    if (id == _specimen_id) {
        return;
    }

    _specimen_id = id;
    _needs.clear();
    _loaded = false;
    _error.clear();
    emit changed();

    // An in-flight completion will refresh the newly selected specimen.
    if (!_busy) {
        refresh();
    }
}

QList<CareNeedOption> Care_Model::needs() const {
    QList<CareNeedOption> result;
    result.reserve(static_cast<int>(_needs.size()));
    for (const auto& need : _needs) {
        const auto entry = std::ranges::find(catalogue, need.type, &Need_Presentation::type);
        if (entry == catalogue.end()) {
            continue;
        }

        auto value = presentation(*entry);
        value.id = QString::number(need.id);
        result.append(value);
    }

    return result;
}

QList<CareNeedOption> Care_Model::available_types() const {
    QList<CareNeedOption> result;
    if (!_loaded) {
        return result;
    }

    result.reserve(catalogue.size());
    for (const auto& entry : catalogue) {
        if (std::ranges::none_of(_needs, [&](const auto& need) { return need.type == entry.type; })) {
            result.append(presentation(entry));
        }
    }

    return result;
}

void Care_Model::refresh() {
    submit({.specimen_id = _specimen_id.toLongLong()});
}

void Care_Model::add(int type) {
    if (!_loaded) {
        return;
    }

    // Duplicate filtering is a UI convention, not a Core invariant.
    bool already_present = std::ranges::any_of(_needs, [type](const auto& need) {
        return static_cast<int>(need.type) == type;
    });

    if (already_present) {
        return;
    }

    submit({
        .kind = mv::CareAction::Kind::Add,
        .specimen_id = _specimen_id.toLongLong(),
        .type = static_cast<mv::NeedType>(type)
    });
}

void Care_Model::remove(const QString& id) {
    submit({
        .kind = mv::CareAction::Kind::Delete,
        .specimen_id = _specimen_id.toLongLong(),
        .id = id.toLongLong()
    });
}

void Care_Model::submit(mv::CareAction action) {
    if (_busy || _specimen_id.isEmpty()) {
        return;
    }

    _busy = true;
    _error.clear();
    emit changed();
    const QPointer<Care_Model> receiver(this);
    const auto specimen = _specimen_id;
    mv::manage_care(action, [receiver, specimen, kind = action.kind](mv::CareResult result) {
        if (!receiver) {
            return;
        }

        receiver->_busy = false;
        if (specimen != receiver->_specimen_id) {
            receiver->refresh();
            return;
        }

        if (result) {
            receiver->_needs  = std::move(*result);
            receiver->_loaded = true;
        } else {
            receiver->_error = ts::get(ts::TextId::Care_Failed);
        }

        emit receiver->changed();
        if (result && kind != mv::CareAction::Kind::List) {
            emit receiver->saved();
        }
    });
}
