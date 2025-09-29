#ifndef SKINS_VIEW_MODEL_HXX
#define SKINS_VIEW_MODEL_HXX

class SkinsViewModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit SkinsViewModel(QObject* parent = nullptr);
};

#endif // SKINS_VIEW_MODEL_HXX
