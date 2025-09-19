#ifndef BACKEND_FACTORY_H
#define BACKEND_FACTORY_H

class BackendFactory : public QObject
{
    Q_OBJECT
public:
    explicit BackendFactory(QObject* parent = nullptr);

signals:
};

#endif // BACKEND_FACTORY_H
