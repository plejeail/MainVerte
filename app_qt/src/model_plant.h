#pragma once

#include <vector>

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

#include "mv_definitions.h"


class PlantModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    Q_PROPERTY(QString selected READ selected NOTIFY changed)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool archived READ archived WRITE showArchived NOTIFY changed)

public:
    enum Action { Browse, CreateCollection, RenameCollection, SelectCollection,
                  DeleteCollection, CreateSpecimen, UpdateSpecimen, MoveSpecimen,
                  ArchiveSpecimen, RestoreSpecimen };
    Q_ENUM(Action)
    explicit PlantModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : static_cast<int>(_visible.size()); }
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariantList collections() const;
    QString selected() const { return QString::number(_plants.selected); }
    int activeCount() const;
    bool archived() const { return _archived; }
    bool busy() const { return _busy; }
    QString error() const { return _error; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void discardSpecimenEditError(const QString& id);
    Q_INVOKABLE void showArchived(bool value);
    Q_INVOKABLE void submit(int kind, const QString& id = {}, const QString& name = {},
                            const QString& botanical = {}, const QString& cultivar = {}, const QString& destination = {},
                            int profile = -1, const QString& acquiredOn = {});
    Q_INVOKABLE QVariantMap specimen(const QString& id) const;
signals:
    void changed();
    void saved();
private:
    void rebuild();
    mv::Plants _plants;
    std::vector<mv::Specimen> _visible;
    bool _busy = false;
    bool _archived = false;
    QString _error;
    int _errorAction = -1;
    QString _errorId;
};
