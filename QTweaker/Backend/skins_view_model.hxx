#ifndef SKINS_VIEW_MODEL_HXX
#define SKINS_VIEW_MODEL_HXX

#include "Backend_global.h"

#include <QModelIndex>

class BACKEND_EXPORT SkinsViewModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit SkinsViewModel(QObject* parent = nullptr);

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
};

#endif // SKINS_VIEW_MODEL_HXX
