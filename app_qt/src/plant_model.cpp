#include "plant_model.h"

#include <algorithm>
#include <utility>

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
                        const QString& botanical, const QString& cultivar, const QString& destination) {
    if (_busy || kind < Browse || kind > RestoreSpecimen) return;

    _busy = true;
    _error.clear();
    emit changed();
    const QPointer<PlantModel> receiver(this);
    mv::manage_plants({static_cast<mv::PlantAction::Kind>(kind), id.toLongLong(), destination.toLongLong(),
                      name.toStdString(), botanical.toStdString(), cultivar.toStdString()},
                     ts::get(ts::TextId::Plants_Default).toStdString(),
                     [receiver, kind](mv::PlantsResult result) {
        if (!receiver) return;
        receiver->_busy = false;
        if (result) {
            receiver->_plants = std::move(*result);
            if (kind == CreateSpecimen) receiver->_archived = false;

            receiver->rebuild();
            if (kind != Browse) emit receiver->saved();
        } else {
            const auto code = result.error().code;
            receiver->_error = ts::get(code == mv::MainverteError::Code::LastCollection
                ? ts::TextId::Plants_LastCollection : code == mv::MainverteError::Code::CollectionNotEmpty
                ? ts::TextId::Plants_NotEmpty : code == mv::MainverteError::Code::InvalidInput
                ? ts::TextId::Plants_NameRequired : ts::TextId::Plants_Failed);
            emit receiver->changed();
        }
    });
}
