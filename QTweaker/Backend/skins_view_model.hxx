#ifndef SKINS_VIEW_MODEL_HXX
#define SKINS_VIEW_MODEL_HXX

#include "Backend_global.h"

#include <QModelIndex>

class SkinItem;

class BACKEND_EXPORT SkinsViewModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        AuthorRole,
        BackgroundRole,
    };

    explicit SkinsViewModel(QObject* parent = nullptr);

    virtual QVariant data(const QModelIndex& index, int role = NameRole) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void addSkin(SkinItem* skin);
    void removeSkin(int index);
    void removeSkin(const QString& name);
    void clear();

    SkinItem* getSkinItem(int index);
    SkinItem* getSkinItem(const QString& name);

private:
    QList<SkinItem*> m_skins;
};

#endif // SKINS_VIEW_MODEL_HXX
