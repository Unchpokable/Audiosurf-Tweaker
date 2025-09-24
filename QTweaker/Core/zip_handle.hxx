#ifndef ZIP_HANDLE_HXX
#define ZIP_HANDLE_HXX

#include "error.hxx"

struct zip;
struct zip_file;
struct zip_source;

typedef zip zip_t;
typedef zip_file zip_file_t;
typedef zip_source zip_source_t;

namespace core::zip
{
class ZipHandle
{
public:
    enum DefaultFinalize
    {
        Save,
        Discard,
    };

    ZipHandle(QStringView path, DefaultFinalize finalize = Save);

    ~ZipHandle();

    core::Error finalize();
    void finalize_discard();

    core::Error error() const;

    QStringView path() const;

    operator zip_t*() {
        return m_archive;
    }

    operator const zip_t*() const {
        return m_archive;
    }

private:
    zip_t* m_archive;
    core::Error m_error;
    QString m_path;

    DefaultFinalize m_finalize_strategy;
    bool m_finalized { false };
};
}
#endif // ZIP_HANDLE_HXX
