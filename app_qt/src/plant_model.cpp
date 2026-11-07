#include "plant_model.h"

#include <algorithm>
#include <utility>

#include <QDate>
#include <QLocale>
#include <QPointer>

#include "translator.h"

namespace {

using CoreAction = mv::PlantAction::Kind;

// QML actions are converted directly to Core actions in submit().
static_assert(PlantModel::Browse == std::to_underlying(CoreAction::Browse));
static_assert(PlantModel::CreateCollection == std::to_underlying(CoreAction::CreateCollection));
static_assert(PlantModel::RenameCollection == std::to_underlying(CoreAction::RenameCollection));
static_assert(PlantModel::SelectCollection == std::to_underlying(CoreAction::SelectCollection));
static_assert(PlantModel::DeleteCollection == std::to_underlying(CoreAction::DeleteCollection));
static_assert(PlantModel::CreateSpecimen == std::to_underlying(CoreAction::CreateSpecimen));
static_assert(PlantModel::UpdateSpecimen == std::to_underlying(CoreAction::UpdateSpecimen));
static_assert(PlantModel::MoveSpecimen == std::to_underlying(CoreAction::MoveSpecimen));
static_assert(PlantModel::ArchiveSpecimen == std::to_underlying(CoreAction::ArchiveSpecimen));
static_assert(PlantModel::RestoreSpecimen == std::to_underlying(CoreAction::RestoreSpecimen));

}  //  namespace

QHash<int, QByteArray> PlantModel::roleNames() const {
    return {{Qt::UserRole, "plantId"}, {Qt::UserRole + 1, "plantName"},
            {Qt::UserRole + 2, "botanical"}, {Qt::UserRole + 3, "cultivar"}};
}
QVariant PlantModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) return {};
    const auto& p = _visible[static_cast<size_t>(index.row())];
    switch (role) {
    case Qt::UserRole: return QString::number(p.id);
    case Qt::UserRole + 1: return QString::fromStdString(p.display_name);
    case Qt::UserRole + 2: return QString::fromStdString(p.botanical_name);
    case Qt::UserRole + 3: return QString::fromStdString(p.cultivar);
    default: return {};
    }
}
int PlantModel::activeCount() const {
    return static_cast<int>(std::ranges::count_if(_plants.specimens, [](const auto& plant) { return !plant.archived; }));
}

QVariantList PlantModel::collections() const {
    QVariantList result;
    for (const auto& c : _plants.collections) {
        result.append(QVariantMap{{QStringLiteral("id"), QString::number(c.id)},
                                  {QStringLiteral("name"), QString::fromStdString(c.name)}});
    }

    return result;
}
void PlantModel::rebuild() {
    beginResetModel();
    _visible.clear();
    for (const auto& p : _plants.specimens) {
        if (p.archived == _archived) _visible.push_back(p);
    }

    endResetModel();
    emit changed();
}
void PlantModel::showArchived(bool value) {
    _archived = value;
    rebuild();
}
void PlantModel::refresh() { submit(Browse); }
void PlantModel::submit(int kind, const QString& id, const QString& name,
                        const QString& botanical, const QString& cultivar, const QString& destination,
                        int profile, const QString& acquiredOn) {
    if (_busy || kind < Browse || kind > RestoreSpecimen) return;

    _busy = true;
    _error.clear();
    emit changed();
    const QPointer<PlantModel> receiver(this);
    mv::manage_plants({static_cast<mv::PlantAction::Kind>(kind), id.toLongLong(), destination.toLongLong(),
                      name.toStdString(), botanical.toStdString(), cultivar.toStdString(),
                      profile < 0 ? std::nullopt : std::optional<mv::PlantProfile>(static_cast<mv::PlantProfile>(profile)),
                      acquiredOn.isEmpty() ? std::nullopt : std::optional<std::string>(acquiredOn.toStdString()),
                      /*replace_optional_metadata=*/true},
                     ts::get(ts::TextId::Plants_Default).toStdString(),
                     [receiver, kind, id, hasName = !name.trimmed().isEmpty()](mv::PlantsResult result) {
        if (!receiver) return;
        receiver->_busy = false;
        if (result) {
            receiver->_plants = std::move(*result);
            if (kind == CreateSpecimen) receiver->_archived = false;

            receiver->rebuild();
            if (kind != Browse) emit receiver->saved();
        } else {
            receiver->_errorAction = kind;
            receiver->_errorId = id;
            const auto code = result.error().code;
            receiver->_error = ts::get(code == mv::MainverteError::Code::LastCollection
                ? ts::TextId::Plants_LastCollection : code == mv::MainverteError::Code::CollectionNotEmpty
                ? ts::TextId::Plants_NotEmpty : code == mv::MainverteError::Code::InvalidInput
                ? (hasName && (kind == UpdateSpecimen || kind == CreateSpecimen) ? ts::TextId::Specimen_InvalidDate : ts::TextId::Plants_NameRequired) : ts::TextId::Plants_Failed);
            emit receiver->changed();
        }
    });
}

QVariantMap PlantModel::specimen(const QString& id) const {
    const auto found = std::ranges::find(_plants.specimens, id.toLongLong(), &mv::Specimen::id);
    if (found == _plants.specimens.end()) return {};

    const auto& plant = *found;
    QString duration;
    QString acquired;
    if (plant.acquired_on) {
        acquired = QString::fromStdString(*plant.acquired_on);
        const auto date = QDate::fromString(acquired, Qt::ISODate);
        const auto today = QDate::currentDate();
        if (date.isValid() && date <= today) {
            int months = (today.year() - date.year()) * 12 + today.month() - date.month();
            if (date.addMonths(months) > today) --months;

            const auto days = date.daysTo(today);
            duration = days == 0 ? ts::get(ts::TextId::Specimen_SinceToday)
                : months >= 12 ? ts::get(ts::TextId::Specimen_SinceYears).replace(QStringLiteral("%1"), QString::number(months / 12))
                : months >= 1 ? ts::get(ts::TextId::Specimen_SinceMonths).replace(QStringLiteral("%1"), QString::number(months))
                : ts::get(ts::TextId::Specimen_SinceDays).replace(QStringLiteral("%1"), QString::number(days));
        }
    }

    return {{QStringLiteral("id"), id}, {QStringLiteral("name"), QString::fromStdString(plant.display_name)},
            {QStringLiteral("botanical"), QString::fromStdString(plant.botanical_name)},
            {QStringLiteral("cultivar"), QString::fromStdString(plant.cultivar)},
            {QStringLiteral("collectionId"), QString::number(plant.collection_id)},
            {QStringLiteral("archived"), plant.archived},
            {QStringLiteral("profile"), plant.profile ? static_cast<int>(*plant.profile) : -1},
            {QStringLiteral("acquiredOn"), acquired}, {QStringLiteral("duration"), duration},
            {QStringLiteral("acquiredLabel"), acquired.isEmpty() ? QString{} : QLocale().toString(QDate::fromString(acquired, Qt::ISODate), QLocale::LongFormat)}};
}

void PlantModel::discardSpecimenEditError(const QString& id) {
    if (_busy || _errorAction != UpdateSpecimen || _errorId != id || _error.isEmpty()) return;

    _error.clear();
    emit changed();
}
