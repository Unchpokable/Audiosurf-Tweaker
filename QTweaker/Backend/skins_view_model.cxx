#include "precompiled.hxx"

#include "skins_view_model.hxx"

#include "skin_changer.hxx"

#include "logging.hxx"

SkinsViewModel::SkinsViewModel(SkinChangerBackend* backend, QObject* parent) : QAbstractListModel(parent),  m_backend(backend)
{
}

QVariant SkinsViewModel::data(const QModelIndex& index, int role) const
{
    return {};
}


int SkinsViewModel::rowCount(const QModelIndex& parent) const
{
    return 0;
}

bool SkinsViewModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
}

void SkinsViewModel::addSkin(SkinItem* skin)
{
}

void SkinsViewModel::removeSkin(int index)
{
}

void SkinsViewModel::clear()
{
}

SkinItem* SkinsViewModel::getSkinItem(int index)
{
    if(index >= 0 && index < m_skins.count()) {
        return m_skins[index];
    }

    LOG_WARNING("SkinsViewModel: requested index out of range: {}", index);
    return nullptr;
}
