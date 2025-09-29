#ifndef COLOR_CONFIGURATOR_HXX
#define COLOR_CONFIGURATOR_HXX

class ColorConfiguratorBackend : public QObject
{
    Q_OBJECT
public:
    explicit ColorConfiguratorBackend(QObject* parent = nullptr);

signals:
};

#endif // COLOR_CONFIGURATOR_HXX
