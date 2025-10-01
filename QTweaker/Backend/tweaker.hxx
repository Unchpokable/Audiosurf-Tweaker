#ifndef TWEAKER_HXX
#define TWEAKER_HXX

#include "Backend_global.h"

class BACKEND_EXPORT TweakerBackend : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit TweakerBackend(QObject* parent = nullptr);
};

Q_DECLARE_METATYPE(TweakerBackend);
Q_DECLARE_METATYPE(TweakerBackend*);

#endif // TWEAKER_HXX
