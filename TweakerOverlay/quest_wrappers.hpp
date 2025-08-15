#pragma once

namespace tw::game
{

class AcoChannelBase
{
public:
    bool is_null() const
    {
        return m_ptr == nullptr;
    }

    operator void*() const
    {
        return m_ptr;
    }

protected:
    void* m_ptr { nullptr };
};

class AcoChannelGroup;

class AcoChannel : public AcoChannelBase
{
public:
    AcoChannel(void* ptr);
    std::string_view get_name() const;
    AcoChannelGroup get_group() const;

    /// returns pointer to data located at m_ptr + field_offset
    void* offset_field(std::ptrdiff_t field_offset) const;
};

class AcoChannelGroup : public AcoChannelBase
{
public:
    AcoChannelGroup(void* ptr);
    std::string_view get_file_name() const;
    std::string_view get_pool_name() const;
    AcoChannel get_channel(const char* name) const;
    AcoChannel get_unique_channel(std::int32_t id) const;
    AcoChannel get_unique_channel(GUID guid) const;
    std::int32_t get_unique_channel_count() const;
};

class AcoEngineInterface : public AcoChannelBase
{
public:
    AcoEngineInterface(void* ptr);
    AcoChannelGroup get_channel_group(std::int32_t id) const;
    std::int32_t get_channel_group_count() const;
};
} // namespace tw::game

namespace tw::game
{
void initialize();
} // namespace tw::game
