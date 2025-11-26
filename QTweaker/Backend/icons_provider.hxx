#pragma once

#include "Backend_global.h"

class BACKEND_EXPORT IconsProvider : public QQuickImageProvider {
    Q_OBJECT

public:
    Q_INVOKABLE explicit IconsProvider(QObject* parent = nullptr) : QQuickImageProvider(QQuickImageProvider::Image) {};

private:
};