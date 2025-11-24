#ifndef ENGINE_H
#define ENGINE_H

#include "Backend_global.h"

#include "appsettings.hxx"
#include "color_configurator.hxx"
#include "skin_changer.hxx"
#include "tweaker.hxx"

class BACKEND_EXPORT Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(SkinChangerBackend* skin_changer READ skin_changer CONSTANT)
    Q_PROPERTY(TweakerBackend* tweaker READ tweaker CONSTANT)
    Q_PROPERTY(ColorConfiguratorBackend* color_configurator READ color_configurator CONSTANT)
    Q_PROPERTY(AppSettingsBackend* settings READ settings CONSTANT)

public:
    Q_INVOKABLE explicit Engine(QObject* parent = nullptr);

    Q_INVOKABLE SkinChangerBackend* skin_changer();
    Q_INVOKABLE TweakerBackend* tweaker();
    Q_INVOKABLE ColorConfiguratorBackend* color_configurator();
    Q_INVOKABLE AppSettingsBackend* settings();

private:
    SkinChangerBackend* m_skin_changer;
    TweakerBackend* m_tweaker;
    ColorConfiguratorBackend* m_color_configurator;
    AppSettingsBackend* m_settings;
};

Q_DECLARE_METATYPE(Engine);
Q_DECLARE_METATYPE(Engine*);

#endif // ENGINE_H
