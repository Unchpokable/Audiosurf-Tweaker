#include "precompiled.hxx"

#include "zip_handle.hxx"

#include "logging.hxx"

core::zip::ZipHandle::ZipHandle(QStringView path, LZ_ModeType mode, DefaultFinalize finalize)
{
    int errp;
    auto zip = zip_open(path.toUtf8().constData(), mode, &errp);

    if(zip == nullptr) {
        zip_error_t libzip_error;
        zip_error_init_with_code(&libzip_error, errp);

        auto error = core::make_error(core::Minor,
            "Archive reading error!",
            "Unable to open archive {}, libzip failed: {}",
            path.toString().toStdString(),
            zip_error_strerror(&libzip_error));

        zip_error_fini(&libzip_error);

        m_error = error;
    }

    m_archive = zip;
    m_path = QString(path);

    m_finalize_strategy = finalize;
}

core::zip::ZipHandle::ZipHandle(ZipHandle&& other)
{
    m_archive = other.m_archive;
    other.m_archive = nullptr;

    m_finalize_strategy = other.m_finalize_strategy;
    m_path = std::move(other.m_path);
    m_error = other.m_error;

    other.m_finalized = true;
}

core::zip::ZipHandle& core::zip::ZipHandle::operator=(ZipHandle&& other)
{
    if(this != &other) {
        if(!m_finalized && m_archive) {
            finalize_impl();
        }

        m_archive = other.m_archive;
        other.m_archive = nullptr;

        m_finalize_strategy = other.m_finalize_strategy;
        m_path = std::move(other.m_path);
        m_error = other.m_error;

        other.m_finalized = true;
    }

    return *this;
}

core::zip::ZipHandle::~ZipHandle()
{
    if(!m_archive) {
        return;
    }

    if(!m_finalized) {
        finalize_impl();
    }
}

core::Error core::zip::ZipHandle::finalize()
{
    if(m_archive) {
        auto code = zip_close(m_archive);

        if(code == -1) {
            auto lz_err = zip_get_error(m_archive);

            auto error =
                core::make_error(core::Moderate, "Archive saving error!", "Unable to write archive: {}", zip_error_strerror(lz_err));

            m_error = error;

            return error;
        }

        m_finalized = true;
    }

    return core::Error {};
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

core::zip::ZipHandle::operator zip_t*()
{
    if(m_error.error_rank != Nothing) {
        LOG_WARNING("Tried to use invalid archive!! Which one: {}", m_path.toStdString());

        return nullptr;
    }

    return m_archive;
}

core::zip::ZipHandle::operator const zip_t*()
{
    if(m_error.error_rank != Nothing) {
        LOG_WARNING("Tried to use invalid archive!! Which one: {}", m_path.toStdString());

        return nullptr;
    }

    return m_archive;
}

void core::zip::ZipHandle::finalize_impl()
{
    switch(m_finalize_strategy) {
        case DefaultFinalize::Discard:
            finalize_discard();
            break;

        case DefaultFinalize::Save:
            {
                auto error = finalize();
                if(error.error_rank != Nothing) {
                    LOG_ERROR("{} : {}", error.short_description.toStdString(), error.detailed_description.toStdString());
                }
                break;
            }
    }
}
