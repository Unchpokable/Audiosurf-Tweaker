#pragma once

#include "Backend_global.h"

class AppSettingsBackend : public QObject {
    Q_OBJECT

public:
    QString textures_root() const;
    void set_textures_root(const QString& data);

private:
    QString m_game_path;
};
