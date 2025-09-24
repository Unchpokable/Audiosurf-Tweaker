#include "precompiled.hxx"

#include "zip_handle.hxx"
#include "logging.hxx"

#include <zip.h>

core::zip::ZipHandle::ZipHandle(QStringView path, DefaultFinalize finalize)
{
    int errp;
    auto zip = zip_open(path.toUtf8().constData(), ZIP_RDONLY, &errp);

    if(zip == nullptr) {
        zip_error_t libzip_error;
        zip_error_init_with_code(&libzip_error, errp);

        auto error = core::make_error(core::Minor, "Archive reading error!", "Unable to open archive {}, libzip failed: {}",
            path.toString().toStdString(), zip_error_strerror(&libzip_error));

        zip_error_fini(&libzip_error);

        m_error = error;
    }

    m_archive = zip;

    m_finalize_strategy = finalize;
}

core::zip::ZipHandle::~ZipHandle()
{
    if(!m_finalized) {

        if(m_finalize_strategy == Save) {
            auto error = finalize();
            if(error.error_rank != Nothing) {
                LOG_ERROR("{} : {}", error.short_description.toStdString(), error.detailed_description.toStdString());
            }
        }
        else if(m_finalize_strategy == Discard) {
            finalize_discard();
        }
    }
}

core::Error core::zip::ZipHandle::finalize()
{
    if(m_archive) {
        auto code = zip_close(m_archive);

        if(code == -1) {
            auto lz_err = zip_get_error(m_archive);

            auto error = core::make_error(core::Moderate, "Archive saving error!", "Unable to write archive: {}", zip_error_strerror(lz_err));

            m_error = error;

            return error;
        }

        m_finalized = true;
    }

    return core::Error{};
}

void core::zip::ZipHandle::finalize_discard()
{
    if(m_archive) {
        zip_discard(m_archive);

        m_finalized = true;
    }
}

core::Error core::zip::ZipHandle::error() const
{
    return m_error;
}

QStringView core::zip::ZipHandle::path() const
{
    return m_path;
}
