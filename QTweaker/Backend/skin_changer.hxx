#ifndef SKIN_CHANGER_H
#define SKIN_CHANGER_H

class SkinChangerBackend : public QObject
{
    Q_OBJECT
public:
    explicit SkinChangerBackend(QObject* parent = nullptr);

signals:
};

#endif // SKIN_CHANGER_H
