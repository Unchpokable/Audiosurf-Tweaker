#ifndef SKIN_CHANGER_H
#define SKIN_CHANGER_H

#include "Backend_global.h"

#include "skin_item.hxx"
#include "skins_view_model.hxx"

class Engine;

class BACKEND_EXPORT SkinChangerBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(SkinsViewModel* model READ model CONSTANT)
    // Configuration properties
    Q_PROPERTY(bool should_install_tiles MEMBER m_install_tiles WRITE config_install_tiles NOTIFY configuration_changed);
    Q_PROPERTY(bool should_install_rings MEMBER m_install_tiles WRITE config_install_rings NOTIFY configuration_changed);
    Q_PROPERTY(bool should_install_hits MEMBER m_install_tiles WRITE config_install_hits NOTIFY configuration_changed);
    Q_PROPERTY(bool should_install_particles MEMBER m_install_tiles WRITE config_install_particles NOTIFY configuration_changed);
    Q_PROPERTY(bool should_install_skyspheres MEMBER m_install_tiles WRITE config_install_skyspheres NOTIFY configuration_changed);
    // UI Binds properties
    Q_PROPERTY(QImageList* selected_previews MEMBER m_selected_previews NOTIFY user_selection_changed);

public:
    Q_INVOKABLE explicit SkinChangerBackend(Engine* engine);

    Q_INVOKABLE SkinsViewModel* model();

    Q_INVOKABLE void add_skin(SkinItem* item);

    Q_INVOKABLE void remove_skin(const QString& name);
    Q_INVOKABLE void remove_skin(int index);

    Q_INVOKABLE void config_install_tiles(bool value);
    Q_INVOKABLE void config_install_rings(bool value);
    Q_INVOKABLE void config_install_hits(bool value);
    Q_INVOKABLE void config_install_particles(bool value);
    Q_INVOKABLE void config_install_skyspheres(bool value);

    Q_INVOKABLE void install_configured(int index);
    Q_INVOKABLE void install_full(int index);

    /* internal API */

    void add_skin(core::PackData& skin_data);

signals:
    void configuration_changed();
    void user_selection_changed();
    void skin_added();
    void skin_removed();

private:
    // Configuration
    bool m_install_tiles;
    bool m_install_rings;
    bool m_install_hits;
    bool m_install_particles;
    bool m_install_skyspheres;

    // UI binds
    SkinsViewModel* m_skins_list_model;
    QImageList* m_selected_previews;

    // Back-to-root
    Engine* m_engine;
};

Q_DECLARE_METATYPE(SkinChangerBackend);
Q_DECLARE_METATYPE(SkinChangerBackend*);

#endif // SKIN_CHANGER_H
