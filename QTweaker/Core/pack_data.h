#ifndef PACK_DATA_H
#define PACK_DATA_H

class PackData : public QObject
{
    Q_OBJECT

public:
    explicit PackData(QObject* parent = nullptr);
};

#endif // PACK_DATA_H
