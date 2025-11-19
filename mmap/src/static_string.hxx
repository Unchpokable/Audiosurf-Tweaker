#pragma once

namespace mmap::memory
{
template<std::size_t Size>
struct StaticString {
    static_assert(Size >= 1, "Size of string buffer should be a non zero!");

    StaticString() noexcept;
    StaticString(const char* c_str) noexcept;
    StaticString(const StaticString& other) noexcept;
    StaticString(StaticString&& other) noexcept;

    StaticString& operator=(const StaticString& other) noexcept;
    StaticString& operator=(StaticString&& other) noexcept;

    const char* c_str() const noexcept;
    std::size_t strlen() const noexcept;

    void clear() noexcept;
    bool empty() const noexcept;

    char* data() noexcept;

    char operator[](std::size_t offset) const noexcept;
    char operator[](std::size_t offset) noexcept;

    bool operator==(const StaticString& other) noexcept;
    bool operator!=(const StaticString& other) noexcept;

    operator const char*() const noexcept;

private:
    char bytes[Size];
};

using WinPathString = StaticString<260>;

} // namespace mmap::memory

template<std::size_t Size>
mmap::memory::StaticString<Size>::operator const char*() const noexcept
{
    return bytes;
}

template<std::size_t Size>
mmap::memory::StaticString<Size>::StaticString() noexcept
{
    std::memset(bytes, 0, Size);
}

template<std::size_t Size>
mmap::memory::StaticString<Size>::StaticString(const char* c_str) noexcept
{
    auto length_to_copy = std::min(Size - 1, std::strlen(c_str));
    std::strncpy(bytes, c_str, length_to_copy);

    bytes[length_to_copy] = '\0';
}

template<std::size_t Size>
mmap::memory::StaticString<Size>::StaticString(const StaticString& other) noexcept
{
    auto length_to_copy = std::min(Size - 1, std::strlen(other));
    std::strncpy(bytes, other.bytes, length_to_copy);

    bytes[length_to_copy] = '\0';
}

template<std::size_t Size>
mmap::memory::StaticString<Size>::StaticString(StaticString&& other) noexcept
{
    auto length_to_copy = std::min(Size - 1, std::strlen(other));
    std::strncpy(bytes, other.bytes, length_to_copy);

    bytes[length_to_copy] = '\0';
}

template<std::size_t Size>
mmap::memory::StaticString<Size>& mmap::memory::StaticString<Size>::operator=(const StaticString& other) noexcept
{
    auto length_to_copy = std::min(Size - 1, std::strlen(other));
    std::strncpy(bytes, other.bytes, length_to_copy);

    bytes[length_to_copy] = '\0';
}

template<std::size_t Size>
mmap::memory::StaticString<Size>& mmap::memory::StaticString<Size>::operator=(StaticString&& other) noexcept
{
    auto length_to_copy = std::min(Size - 1, std::strlen(other));
    std::strncpy(bytes, other.bytes, length_to_copy);

    bytes[length_to_copy] = '\0';
}

template<std::size_t Size>
const char* mmap::memory::StaticString<Size>::c_str() const noexcept
{
    return bytes;
}

template<std::size_t Size>
std::size_t mmap::memory::StaticString<Size>::strlen() const noexcept
{
    return std::strlen(bytes);
}

template<std::size_t Size>
void mmap::memory::StaticString<Size>::clear() noexcept
{
    std::memset(bytes, 0, Size);
}

template<std::size_t Size>
bool mmap::memory::StaticString<Size>::empty() const noexcept
{
    return bytes[0] == '\0';
}

template<std::size_t Size>
char* mmap::memory::StaticString<Size>::data() noexcept
{
    return bytes;
}

template<std::size_t Size>
char mmap::memory::StaticString<Size>::operator[](std::size_t offset) const noexcept
{
    if(offset < 0 || offset >= Size) {
        return '\0';
    }

    return bytes[offset];
}

template<std::size_t Size>
char mmap::memory::StaticString<Size>::operator[](std::size_t offset) noexcept
{
    if(offset < 0 || offset >= Size) {
        return '\0';
    }

    return bytes[offset];
}

template<std::size_t Size>
bool mmap::memory::StaticString<Size>::operator==(const StaticString& other) noexcept
{
    return std::strcmp(bytes, other.bytes) == 0;
}

template<std::size_t Size>
bool mmap::memory::StaticString<Size>::operator!=(const StaticString& other) noexcept
{
    return std::strcmp(bytes, other.bytes) != 0;
}
