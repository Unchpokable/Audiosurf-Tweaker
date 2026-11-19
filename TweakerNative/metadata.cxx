#include "metadata.hxx"

#include <memory>
#include <string>

#include <fileref.h>
#include <flacproperties.h>
#include <mp4properties.h>
#include <tag.h>
#include <tpropertymap.h>
#include <wavproperties.h>

namespace
{
struct metadata_snapshot {
    std::wstring title;
    std::wstring artist;
    std::wstring album;
};

std::int32_t bit_depth(const TagLib::AudioProperties* properties)
{
    if(const auto* flac = dynamic_cast<const TagLib::FLAC::Properties*>(properties)) {
        return flac->bitsPerSample();
    }
    if(const auto* wav = dynamic_cast<const TagLib::RIFF::WAV::Properties*>(properties)) {
        return wav->bitsPerSample();
    }
    if(const auto* mp4 = dynamic_cast<const TagLib::MP4::Properties*>(properties)) {
        return mp4->bitsPerSample();
    }

    return 0;
}
} // namespace

std::int32_t read_metadata(const wchar_t* path, tweaker_metadata* result, void** snapshot) noexcept
{
    if(path == nullptr || result == nullptr || snapshot == nullptr) {
        return 1;
    }

    *snapshot = nullptr;
    *result = {};
    try {
        const TagLib::FileRef file(path, true, TagLib::AudioProperties::Accurate);
        if(file.isNull() || !file.file()->isValid() || file.tag() == nullptr || file.audioProperties() == nullptr
            || file.audioProperties()->sampleRate() <= 0) {
            return 1;
        }

        auto data = std::make_unique<metadata_snapshot>();
        data->title = file.tag()->title().toWString();
        data->album = file.tag()->album().toWString();
        const auto* properties = file.audioProperties();
        const auto artists = file.file()->properties()["ARTIST"];
        // WAV may have an ID3 title and an artist only in RIFF INFO. TagUnion's
        // generic accessor falls back across tags; its property map uses the first tag.
        auto artist = artists.isEmpty() ? file.tag()->artist() : artists.front();
        if(dynamic_cast<const TagLib::MP4::Properties*>(properties) != nullptr) {
            // Preserve TagLibSharp AppleTag's first-performer semantics for semicolon
            // lists, without splitting literal semicolons in other containers' artist names.
            const auto separator = artist.find(';');
            if(separator >= 0) {
                artist = artist.substr(0, separator);
            }
            artist = artist.stripWhiteSpace();
        }
        data->artist = artist.toWString();

        *result = {
            data->title.c_str(),
            data->artist.c_str(),
            data->album.c_str(),
            properties->lengthInMilliseconds(),
            bit_depth(properties),
            properties->sampleRate(),
            properties->bitrate()
        };
        *snapshot = data.release();
        return 0;
    }
    catch(...) {
        return 2;
    }
}

void free_metadata(void* snapshot) noexcept
{
    delete static_cast<metadata_snapshot*>(snapshot);
}

std::int32_t write_title(const wchar_t* path, const wchar_t* title) noexcept
{
    if(path == nullptr || title == nullptr) {
        return 1;
    }

    try {
        TagLib::FileRef file(path, true, TagLib::AudioProperties::Accurate);
        if(file.isNull() || !file.file()->isValid() || file.tag() == nullptr || file.file()->readOnly()
            || file.audioProperties() == nullptr || file.audioProperties()->sampleRate() <= 0) {
            return 1;
        }

        file.tag()->setTitle(TagLib::String(title));
        return file.save() ? 0 : 3;
    }
    catch(...) {
        return 2;
    }
}
