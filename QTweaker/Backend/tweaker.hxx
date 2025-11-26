#ifndef TWEAKER_HXX
#define TWEAKER_HXX

#include "Backend_global.h"

class Engine;

class BACKEND_EXPORT TweakerBackend : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE explicit TweakerBackend(Engine* engine);

private:
    Engine* m_engine;
};

Q_DECLARE_METATYPE(TweakerBackend);
Q_DECLARE_METATYPE(TweakerBackend*);

#endif // TWEAKER_HXX
