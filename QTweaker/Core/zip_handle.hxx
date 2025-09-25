#ifndef ZIP_HANDLE_HXX
#define ZIP_HANDLE_HXX

#include "error.hxx"
#include <zip.h>

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
    using LZ_ModeType = decltype(ZIP_RDONLY);

public:
    enum class DefaultFinalize
    {
        Save,
        Discard,
    };

    ZipHandle(QStringView path, LZ_ModeType mode, DefaultFinalize finalize = DefaultFinalize::Save);

    ZipHandle(const ZipHandle& other) = delete;
    ZipHandle& operator=(const ZipHandle& other) = delete;

    ZipHandle(ZipHandle&& other);
    ZipHandle& operator=(ZipHandle&& other);

    ~ZipHandle();

    core::Error finalize();
    void finalize_discard();

    core::Error error() const;

    QStringView path() const;

    operator zip_t*();

    operator const zip_t*();

private:
    void finalize_impl();

    zip_t* m_archive;
    core::Error m_error;
    QString m_path;

    DefaultFinalize m_finalize_strategy;
    bool m_finalized { false };
};
}
#endif // ZIP_HANDLE_HXX
