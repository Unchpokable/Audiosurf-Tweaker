#ifndef PACKAGER_H
#define PACKAGER_H

class Packager : public QObject
{
    Q_OBJECT

public:
    explicit Packager(QObject* parent = nullptr);

private:
    static constexpr char m_file_header[] = "TWEAKER_SKIN0000";
    static constexpr std::uint16_t version[3] = { 1, 0, 0 };
};

#endif // PACKAGER_H
