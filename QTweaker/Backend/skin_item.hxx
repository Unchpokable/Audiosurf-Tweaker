#ifndef SKIN_ITEM_H
#define SKIN_ITEM_H

#include "precompiled.hxx"

#include "pack_data.hxx"

using QImageList = QList<QImage>;

class SkinItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(
        QString name READ name WRITE rename NOTIFY renamed)
    Q_PROPERTY(
        QString author READ author WRITE set_author NOTIFY author_changed)
    Q_PROPERTY(
        QImageList previews READ previews CONSTANT)

public:
    explicit SkinItem(QObject* parent = nullptr);

    virtual ~SkinItem() override = default;

    Q_INVOKABLE const QString& name() const noexcept
    {
        return m_data.name;
    }

    Q_INVOKABLE const QString& author() const noexcept
    {
        return m_data.author;
    }

    core::PackData& data()
    {
        return m_data;
    }

    void rename(const QString& name);
    void set_author(const QString& author);

    QImageList previews();

signals:
    void renamed(QString new_name);
    void author_changed(QString new_author);

private:
    QImage::Format deduce_format(const core::RawImage& image) const;

    core::PackData m_data;
};

#endif
