#pragma once

namespace tw::game
{
class AcoChannelGroup;

class AcoChannel
{
public:
    AcoChannel(void* ptr);
    std::string_view get_name() const;
    AcoChannelGroup get_group() const;

    /// returns pointer to data located at m_ptr + field_offset
    void* offset_field(std::ptrdiff_t field_offset) const;

private:
    void* m_ptr { nullptr };
};

class AcoChannelGroup
{
public:
    AcoChannelGroup(void* ptr);
    std::string_view get_file_name() const;
    std::string_view get_pool_name() const;
    AcoChannel get_channel(const char* name) const;

private:
    void* m_ptr { nullptr };
};

class AcoEngineInterface
{
public:
    AcoEngineInterface(void* ptr);
    AcoChannelGroup get_channel_group(std::int32_t id) const;
    std::int32_t get_channel_group_count() const;

private:
    void* m_ptr { nullptr };
};
} // namespace tw::game

namespace tw::game
{
void initialize();
} // namespace tw::game
