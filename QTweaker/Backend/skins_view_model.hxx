#ifndef SKINS_VIEW_MODEL_HXX
#define SKINS_VIEW_MODEL_HXX

#include "Backend_global.h"

#include <QModelIndex>

class SkinItem;
class SkinChangerBackend;

class BACKEND_EXPORT SkinsViewModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        BackgroundRole,
    };

    explicit SkinsViewModel(SkinChangerBackend* backend, QObject* parent = nullptr);

    virtual QVariant data(const QModelIndex &index, int role = NameRole) const override;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void addSkin(SkinItem* skin);
    void removeSkin(int index);
    void clear();

    SkinItem* getSkinItem(int index);

private:
    QList<SkinItem*> m_skins;

    SkinChangerBackend* m_backend;
};

#endif // SKINS_VIEW_MODEL_HXX
