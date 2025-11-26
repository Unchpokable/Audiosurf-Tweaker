#include "precompiled.hxx"

#include "skins_view_model.hxx"

#include "image_utils.hxx"
#include "logging.hxx"
#include "skin_item.hxx"

SkinsViewModel::SkinsViewModel(QObject* parent) : QAbstractListModel(parent)
{
}

QVariant SkinsViewModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return {};
    }

    auto object = m_skins[index.row()];

    if(role == NameRole) {
        return object->name();
    }
    else if(role == AuthorRole) {
        return object->author();
    }

    return {};
}

int SkinsViewModel::rowCount(const QModelIndex& parent) const
{
    return m_skins.count();
}

QHash<int, QByteArray> SkinsViewModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { AuthorRole, "author" },
    };
}

bool SkinsViewModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid()) {
        return false;
    }

    auto object = m_skins[index.row()];

    if(role == NameRole) {
        object->rename(value.toString());
        return true;
    }
    else if(role == AuthorRole) {
        object->set_author(value.toString());
        return true;
    }

    return false;
}

Qt::ItemFlags SkinsViewModel::flags(const QModelIndex& index) const
{
    if(!index.isValid())
        return Qt::NoItemFlags;

    auto base_flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    return base_flags;
}

void SkinsViewModel::addSkin(SkinItem* skin)
{
    assert(skin);

    skin->setParent(this);
    auto position = m_skins.size();

    beginInsertRows(QModelIndex(), position, position);
    m_skins.append(skin);
    endInsertRows();
}

void SkinsViewModel::removeSkin(int index)
{
    if(index < 0 || index >= m_skins.size()) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_skins.erase(m_skins.begin() + index);
    endRemoveRows();
}

void SkinsViewModel::removeSkin(const QString& name)
{
    auto target = std::ranges::find(m_skins, name, &SkinItem::name);
    if(target != std::ranges::end(m_skins)) {
        auto index = std::ranges::distance(m_skins.begin(), target);
        removeSkin(index);
    }
}

void SkinsViewModel::clear()
{
    beginResetModel();
    m_skins.clear();
    endResetModel();
}

SkinItem* SkinsViewModel::getSkinItem(int index)
{
    if(index >= 0 && index < m_skins.count()) {
        return m_skins[index];
    }

    LOG_WARNING("SkinsViewModel: requested index out of range: {}", index);
    return nullptr;
}
