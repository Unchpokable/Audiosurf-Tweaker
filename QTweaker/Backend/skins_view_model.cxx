#include "precompiled.hxx"

#include "skins_view_model.hxx"

SkinsViewModel::SkinsViewModel(QObject* parent) : QAbstractListModel(parent)
{
}

int SkinsViewModel::rowCount(const QModelIndex& parent) const
{
    return 0;
}

QVariant SkinsViewModel::data(const QModelIndex& index, int role) const
{
    return {};
}
