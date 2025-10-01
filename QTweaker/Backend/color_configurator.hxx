#ifndef COLOR_CONFIGURATOR_HXX
#define COLOR_CONFIGURATOR_HXX

#include "Backend_global.h"

class BACKEND_EXPORT ColorConfiguratorBackend : public QObject
{
    Q_OBJECT
public:
    explicit ColorConfiguratorBackend(QObject* parent = nullptr);

signals:
};

Q_DECLARE_METATYPE(ColorConfiguratorBackend);
Q_DECLARE_METATYPE(ColorConfiguratorBackend*);

#endif // COLOR_CONFIGURATOR_HXX
