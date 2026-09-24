#include "pch.hxx"

#include "engine/engine_groups.hxx"

#include "engine/engine_frame.hxx"
#include "engine/engine_symbols.hxx"

#include "framework/channel_shim.hxx"

#include "plugin/boot_log.hxx"
#include "plugin/diagnostics.hxx"
#include "plugin/quest3d_state.hxx"

namespace
{
using tw::engine::groups::generation;

// 161 is the whole project (reversing-journal-boot.md §5), and the game never has all of them at
// once. The cap is a sanity clamp on a number read out of the engine, not a design limit.
constexpr int k_max_groups = 512;

// Pool names are short identifiers ("Renderer", "StatCollector"); file names are what the .cgr
// records hold, which can be a relative path (".\Render\Render1.cgr").
constexpr std::size_t k_pool_name_size = 64;
constexpr std::size_t k_file_name_size = 160;

constexpr int k_max_listeners = 8;

// EngineInterface::ext_ - the EngineInterfaceExt every one of its accessors forwards to. Read off
// the disassembly of `?GetGraphRunning@EngineInterface@@UAE_NXZ`, which is `mov eax,[ecx+0x40]` and
// a null check before the forward (boot journal §1.3).
//
// **An offset is a thing that can be wrong, so this one is checked rather than trusted**: the
// pointer it yields is only ever used after its vptr matched the exported
// `??_7EngineInterfaceExt@@6B@`. A wrong offset therefore costs the start-group signal, not the
// process.
constexpr std::size_t k_engine_interface_ext_offset = 0x40;

struct entry {
    A3d_ChannelGroup* group;
    generation gen;
    char pool[k_pool_name_size];
    char file[k_file_name_size];
};

std::vector<entry> g_roster;

// Serial numbers, never reused. Starts at 1 so that 0 stays available as "no group".
generation g_next_generation = 1;

std::uint32_t g_revision = 0;
std::uint32_t g_last_change_frame = 0;
std::uint64_t g_last_change_ms = 0;

bool g_available = false;
bool g_hooks_installed = false;

// Set by the unload detour, acted on by the next tick(). See handle_unload().
bool g_notify_pending = false;

// Verified once, then reused: the check is a pointer compare, but getting there costs a load through
// an offset we would rather not repeat on every frame.
tw::engine::engine_interface_ext_handle* g_ext = nullptr;
bool g_ext_checked = false;

std::array<tw::engine::groups::changed_fn, k_max_listeners> g_changed {};
std::array<tw::engine::groups::unloading_fn, k_max_listeners> g_unloading {};
int g_changed_count = 0;
int g_unloading_count = 0;

tw::engine::symbols::group_release_fn o_group_release = nullptr;
tw::engine::symbols::delete_group_fn o_delete_channel_group = nullptr;

std::string g_start_group_file;

void copy_name(char* destination, std::size_t size, const char* source) noexcept
{
    if(source == nullptr) {
        destination[0] = '\0';
        return;
    }

    std::size_t i = 0;
    for(; i + 1 < size && source[i] != '\0'; ++i) {
        destination[i] = source[i];
    }

    destination[i] = '\0';
}

// "Puzzle" out of ".\Groups\Puzzle.cgr" - the name anyone reading the journals will type, as opposed
// to the path the engine stores.
std::string_view bare_name(const char* file) noexcept
{
    if(file == nullptr) {
        return {};
    }

    std::string_view view { file };

    if(const std::size_t slash = view.find_last_of("\\/"); slash != std::string_view::npos) {
        view.remove_prefix(slash + 1);
    }

    if(view.size() > 4 && ::_strnicmp(view.data() + view.size() - 4, ".cgr", 4) == 0) {
        view.remove_suffix(4);
    }

    return view;
}

bool equals_ignore_case(std::string_view a, const char* b) noexcept
{
    if(b == nullptr) {
        return false;
    }

    const std::string_view other { b };

    return a.size() == other.size() && ::_strnicmp(a.data(), other.data(), a.size()) == 0;
}

entry* find_entry(A3d_ChannelGroup* group) noexcept
{
    for(entry& item : g_roster) {
        if(item.group == group) {
            return &item;
        }
    }

    return nullptr;
}

void stamp_change() noexcept
{
    ++g_revision;
    g_last_change_frame = tw::engine::frame::count();
    g_last_change_ms = ::GetTickCount64();
}

void notify_changed() noexcept
{
    for(int i = 0; i < g_changed_count; ++i) {
        g_changed[i]();
    }
}

// Rebuilds the roster from the engine's own list, keeping the generation of every group that was
// already there. Cold path: only reached when the count moved.
void rebuild() noexcept
{
    EngineInterface* const engine = tw::plugin::quest3d::g_engine;
    const tw::engine::symbols::group_table& api = tw::engine::symbols::groups();

    const auto get_count = reinterpret_cast<tw::engine::symbols::get_group_count_fn>(api.get_group_count);
    const auto get_at = reinterpret_cast<tw::engine::symbols::get_group_at_fn>(api.get_group_at);
    const auto get_pool = reinterpret_cast<tw::engine::symbols::group_name_fn>(api.get_pool_name);
    const auto get_file = reinterpret_cast<tw::engine::symbols::group_name_fn>(api.get_group_file_name);

    const int count = std::min(get_count(engine, nullptr), k_max_groups);

    std::vector<entry> next;
    next.reserve(static_cast<std::size_t>(std::max(count, 0)));

    for(int i = 0; i < count; ++i) {
        A3d_ChannelGroup* const group = get_at(engine, nullptr, i);
        if(group == nullptr) {
            continue;
        }

        // A group already in the roster keeps its serial number. One that is not is new by
        // definition: the unload detour takes an entry out the moment its group starts being
        // destroyed, so nothing that survived that can be mistaken for something that came back.
        const entry* const existing = find_entry(group);

        entry item {};
        item.group = group;
        item.gen = existing != nullptr ? existing->gen : g_next_generation++;
        copy_name(item.pool, k_pool_name_size, get_pool(group, nullptr));
        copy_name(item.file, k_file_name_size, get_file(group, nullptr));

        next.push_back(item);
    }

    g_roster.swap(next);
}

void handle_unload(A3d_ChannelGroup* group) noexcept
{
    if(group == nullptr || g_roster.empty()) {
        return;
    }

    const entry* const item = find_entry(group);
    if(item == nullptr) {
        // Either a group we never saw, or the second path into the same destruction - Remove Group
        // goes through DeleteChannelGroup and the engine's own teardown through Release, and nothing
        // says the two cannot both run. Taking the entry out first makes the second one a no-op.
        return;
    }

    const generation gen = item->gen;

    // **The vtable copies come off first, before any listener and before the original.** This is the
    // ordering the offline suite mutates on purpose: leave it until after the original and the
    // engine's own channel destructors run through our copies, and a copy that outlives its object
    // is a jump into freed memory rather than a stale read.
    //
    // It is done here rather than left to whoever installed the shims, because correctness of the
    // ordering must not depend on a listener being registered, or on the order two of them were
    // registered in.
    const int restored = tw::framework::channel_shim::remove_all_in(group);
    if(restored > 0) {
        TW_LOG_INFO("engine_groups: group {} unloading - {} channel(s) unhooked", static_cast<void*>(group), restored);
    }

    // Listeners next, while the group is still whole, so each can forget what it cached.
    for(int i = 0; i < g_unloading_count; ++i) {
        g_unloading[i](group, gen);
    }

    std::erase_if(g_roster, [group](const entry& candidate) { return candidate.group == group; });

    // The change is stamped here - a handle comparing revisions must see it immediately, or it could
    // read through a pointer into this group for the rest of the frame.
    //
    // **The `changed` listeners are deliberately NOT called from here.** They are the ones that go
    // looking for groups to attach to, and doing that from inside a group's own destructor means
    // asking the engine for a group that is being destroyed right now - and possibly getting it,
    // since it is still in the engine's list until the original returns. A shim installed on one of
    // its channels at that moment would be installed after remove_all_in() had already run, and
    // would outlive the object.
    //
    // Deferred to the next tick() instead, through this flag rather than through the count
    // comparison - and the flag is not belt-and-braces, it is the only thing that works. By the time
    // the next frame comes round the engine has taken the group out of its own list too, so its
    // count and the roster's size agree again and the cheap comparison sees nothing at all.
    stamp_change();
    g_notify_pending = true;
}

void __fastcall hk_group_release(A3d_ChannelGroup* self, void* /*edx*/)
{
    handle_unload(self);
    o_group_release(self, nullptr);
}

void __fastcall hk_delete_channel_group(EngineInterface* self, void* /*edx*/, int index)
{
    // The narrow path hands over an index, so the group has to be asked for by index before it goes.
    // Safe here and nowhere later: this runs before the original, with the engine's list intact.
    if(self != nullptr && tw::engine::symbols::groups_ready()) {
        const auto get_at = reinterpret_cast<tw::engine::symbols::get_group_at_fn>(tw::engine::symbols::groups().get_group_at);
        handle_unload(get_at(self, nullptr, index));
    }

    o_delete_channel_group(self, nullptr, index);
}

// EngineInterface::ext_, once its vptr has been matched against the exported vtable. Null when the
// engine is not captured yet, or when the offset did not lead to an EngineInterfaceExt - in which
// case the start-group signal is simply unavailable and everything else here still works.
tw::engine::engine_interface_ext_handle* engine_ext() noexcept
{
    if(g_ext_checked) {
        return g_ext;
    }

    EngineInterface* const engine = tw::plugin::quest3d::g_engine;
    if(engine == nullptr) {
        return nullptr;
    }

    g_ext_checked = true;

    auto* const candidate = *reinterpret_cast<tw::engine::engine_interface_ext_handle**>(
        reinterpret_cast<std::byte*>(engine) + k_engine_interface_ext_offset);
    if(candidate == nullptr) {
        TW_LOG_WARNING("engine_groups: EngineInterface+0x{:x} is null - no start-group signal this session",
            k_engine_interface_ext_offset);
        return nullptr;
    }

    const void* const vptr = *reinterpret_cast<void* const*>(candidate);
    const void* const expected = tw::engine::symbols::groups().engine_interface_ext_vtable;
    if(vptr != expected) {
        TW_BOOT_LOG("engine: EngineInterface+0x{:x} has vptr {}, EngineInterfaceExt's vtable is {} - "
                    "state falls back to group settling alone",
            k_engine_interface_ext_offset,
            vptr,
            expected);
        TW_LOG_WARNING("engine_groups: vptr {} is not EngineInterfaceExt's vtable {} - no start-group signal", vptr, expected);
        return nullptr;
    }

    g_ext = candidate;
    TW_LOG_INFO("engine_groups: EngineInterfaceExt {} verified - start group readable", static_cast<void*>(candidate));

    return g_ext;
}
} // namespace

namespace tw::engine::groups
{
bool initialize() noexcept
{
    if(g_available) {
        return true;
    }

    if(!symbols::initialize() || !symbols::groups_ready()) {
        return false;
    }

    g_roster.reserve(k_max_groups / 2);
    g_available = true;

    return true;
}

bool available() noexcept
{
    return g_available;
}

bool install_hooks(framework::detour::suspend threads) noexcept
{
    if(g_hooks_installed) {
        return true;
    }

    if(!initialize()) {
        return false;
    }

    o_group_release = reinterpret_cast<symbols::group_release_fn>(symbols::groups().group_release);
    o_delete_channel_group = reinterpret_cast<symbols::delete_group_fn>(symbols::groups().delete_channel_group);

    const bool ok = framework::detour::attach(
        {
            { reinterpret_cast<void**>(&o_group_release), reinterpret_cast<void*>(hk_group_release) },
            { reinterpret_cast<void**>(&o_delete_channel_group), reinterpret_cast<void*>(hk_delete_channel_group) },
        },
        threads);

    if(!ok) {
        o_group_release = nullptr;
        o_delete_channel_group = nullptr;
        return false;
    }

    g_hooks_installed = true;

    return true;
}

bool hooks_installed() noexcept
{
    return g_hooks_installed;
}

void tick() noexcept
{
    if(!g_available) [[unlikely]] {
        return;
    }

    EngineInterface* const engine = tw::plugin::quest3d::g_engine;
    if(engine == nullptr) [[unlikely]] {
        return;
    }

    // The whole per-frame cost of the registry: one call and one comparison. Compared against the
    // roster's own size rather than a separately cached count, so that an unload and a load inside
    // the same frame - which leaves the engine's count exactly where it was - still comes out as a
    // difference, because the unload detour has already taken its entry away.
    const auto get_count = reinterpret_cast<symbols::get_group_count_fn>(symbols::groups().get_group_count);
    const bool moved = get_count(engine, nullptr) != static_cast<int>(g_roster.size());

    if(!moved && !g_notify_pending) [[likely]] {
        return;
    }

    // An unload rebuilds too, not only re-announces. The roster was edited from inside the game's
    // teardown on the strength of one pointer; this is the first safe moment to reconcile it with
    // what the engine actually lists, which is the only authority on what is loaded.
    g_notify_pending = false;
    rebuild();

    if(moved) {
        stamp_change();
    }

    notify_changed();
}

int count() noexcept
{
    return static_cast<int>(g_roster.size());
}

std::uint32_t revision() noexcept
{
    return g_revision;
}

std::uint32_t last_change_frame() noexcept
{
    return g_last_change_frame;
}

std::uint64_t last_change_ms() noexcept
{
    return g_last_change_ms;
}

A3d_ChannelGroup* start_group() noexcept
{
    if(!g_available) {
        return nullptr;
    }

    engine_interface_ext_handle* const ext = engine_ext();
    if(ext == nullptr) {
        return nullptr;
    }

    EngineInterface* const engine = tw::plugin::quest3d::g_engine;
    if(engine == nullptr) {
        return nullptr;
    }

    const auto get_start = reinterpret_cast<symbols::get_start_group_fn>(symbols::groups().get_start_group);
    const auto get_at = reinterpret_cast<symbols::get_group_at_fn>(symbols::groups().get_group_at);

    const int index = get_start(ext, nullptr);
    if(index < 0) {
        return nullptr;
    }

    return get_at(engine, nullptr, index);
}

const char* start_group_file() noexcept
{
    g_start_group_file.clear();

    A3d_ChannelGroup* const group = start_group();
    if(group == nullptr) {
        return g_start_group_file.c_str();
    }

    // Out of the roster when it is there, and only otherwise from the engine: the start group can be
    // set to one the count-driven rebuild has not caught up with yet.
    if(const entry* const item = find_entry(group); item != nullptr) {
        g_start_group_file.assign(item->file);
        return g_start_group_file.c_str();
    }

    const auto get_file = reinterpret_cast<symbols::group_name_fn>(symbols::groups().get_group_file_name);
    if(const char* const file = get_file(group, nullptr); file != nullptr) {
        g_start_group_file.assign(file);
    }

    return g_start_group_file.c_str();
}

A3d_ChannelGroup* find(const char* name) noexcept
{
    if(name == nullptr || name[0] == '\0') {
        return nullptr;
    }

    // Three spellings, most specific first. The full stored file name is what the engine's own
    // GetChannelGroup(const char*) matches, and the only one that can tell two groups loaded from
    // different directories under the same bare name apart; the pool name is what .cgr cross-group
    // records use; the bare file name is what anyone reading the journals will type.
    for(const entry& item : g_roster) {
        if(::_stricmp(item.file, name) == 0) {
            return item.group;
        }
    }

    for(const entry& item : g_roster) {
        if(::_stricmp(item.pool, name) == 0) {
            return item.group;
        }
    }

    for(const entry& item : g_roster) {
        if(equals_ignore_case(bare_name(item.file), name)) {
            return item.group;
        }
    }

    return nullptr;
}

generation generation_of(A3d_ChannelGroup* group) noexcept
{
    const entry* const item = find_entry(group);
    return item != nullptr ? item->gen : no_generation;
}

bool alive(A3d_ChannelGroup* group, generation gen) noexcept
{
    if(gen == no_generation) {
        return false;
    }

    const entry* const item = find_entry(group);

    return item != nullptr && item->gen == gen;
}

const char* pool_name_of(A3d_ChannelGroup* group) noexcept
{
    const entry* const item = find_entry(group);
    return item != nullptr ? item->pool : "";
}

const char* file_name_of(A3d_ChannelGroup* group) noexcept
{
    const entry* const item = find_entry(group);
    return item != nullptr ? item->file : "";
}

int roster_size() noexcept
{
    return static_cast<int>(g_roster.size());
}

A3d_ChannelGroup* roster_at(int index) noexcept
{
    if(index < 0 || index >= static_cast<int>(g_roster.size())) {
        return nullptr;
    }

    return g_roster[static_cast<std::size_t>(index)].group;
}

void subscribe_changed(changed_fn fn) noexcept
{
    if(fn == nullptr || g_changed_count >= k_max_listeners) {
        return;
    }

    g_changed[g_changed_count++] = fn;
}

void subscribe_unloading(unloading_fn fn) noexcept
{
    if(fn == nullptr || g_unloading_count >= k_max_listeners) {
        return;
    }

    g_unloading[g_unloading_count++] = fn;
}
} // namespace tw::engine::groups
