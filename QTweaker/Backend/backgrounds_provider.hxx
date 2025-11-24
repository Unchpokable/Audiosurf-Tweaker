#pragma once

class Engine;

class BackgroundsProvider : public QQuickImageProvider {
    Q_OBJECT

public:
    BackgroundsProvider(Engine* engine);

    virtual QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

public slots:
    void invalidateObjectBackgroundCache(const QString& id);

private:
    Engine* m_engine;

    QHash<QString&, QImage> m_cached;
};
