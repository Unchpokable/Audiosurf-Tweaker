#include "pch.hxx"

#include "skybox/sky_reload.hxx"

#include "plugin/bg_work.hxx"

#include "skybox/sky_package.hxx"

namespace
{
struct stamp {
    std::filesystem::path path;

    std::filesystem::file_time_type time {};
    std::uintmax_t size {};
    bool exists {};

    bool operator==(const stamp&) const = default;
};

struct result {
    std::vector<stamp> stamps;

    tw::skybox::reload::update value;
};

std::filesystem::path g_path;
std::vector<stamp> g_stamps;
std::shared_ptr<const tw::skybox::package::manifest> g_known;

tw::plugin::bg_work::task<result> g_pending;

std::chrono::steady_clock::time_point g_last_poll {};
bool g_requested = false;

stamp inspect(const std::filesystem::path& path)
{
    stamp out;
    out.path = path;

    std::error_code ec;
    out.exists = std::filesystem::exists(path, ec);

    if(out.exists) {
        out.time = std::filesystem::last_write_time(path, ec);
        if(std::filesystem::is_regular_file(path, ec)) {
            out.size = std::filesystem::file_size(path, ec);
        }
    }

    return out;
}

result read(const std::filesystem::path& root,
    const std::vector<stamp>& previous,
    const std::shared_ptr<const tw::skybox::package::manifest>& known)
{
    result out;

    const auto add = [&out](const std::filesystem::path& path) {
        if(std::ranges::none_of(out.stamps, [&path](const stamp& s) {
               return s.path == path;
           })) {
            out.stamps.push_back(inspect(path));
        }
    };

    add(root);

    std::error_code ec;
    if(std::filesystem::is_directory(root, ec)) {
        add(root / L"Config.json");
    }

    if(known != nullptr) {
        for(const auto& path : known->watched) {
            add(path);
        }
    }

    if(out.stamps == previous) {
        return out;
    }

    // Stamp before reading: a replacement during the read must be noticed by the next check.
    // Keep individual stamps, including missing files, rather than only the maximum: deleting a
    // shader or changing an older file must also reload a directory package.
    out.value.missing = !out.stamps.front().exists;
    out.value.sky = std::make_shared<tw::skybox::package::manifest>(tw::skybox::package::load(root));

    // Rebuild the watch set in its canonical order. Carry the pre-read stamps for known paths;
    // removed manifest references must not cause a second reload on the following check.
    auto before = std::move(out.stamps);
    out.stamps.clear();

    const auto keep = [&out, &before](const std::filesystem::path& path) {
        if(std::ranges::any_of(out.stamps, [&path](const stamp& s) {
               return s.path == path;
           })) {
            return;
        }

        const auto old = std::ranges::find_if(before, [&path](const stamp& s) {
            return s.path == path;
        });
        out.stamps.push_back(old != before.end() ? *old : inspect(path));
    };

    keep(root);

    if(std::filesystem::is_directory(root, ec)) {
        keep(root / L"Config.json");
    }

    for(const auto& path : out.value.sky->watched) {
        keep(path);
    }

    return out;
}
} // namespace

namespace tw::skybox::reload
{
void clear() noexcept
{
    g_pending.abandon();

    g_path.clear();
    g_stamps.clear();
    g_known.reset();

    g_requested = false;
}

void select(std::filesystem::path path)
{
    clear();

    g_path = std::move(path);

    request();
}

void request() noexcept
{
    g_requested = !g_path.empty();
}

bool loading() noexcept
{
    return !g_path.empty() && g_known == nullptr;
}

std::optional<update> poll(bool watch)
{
    std::optional<update> ready;

    if(auto finished = g_pending.take(); finished.has_value()) {
        g_stamps = std::move(finished->stamps);

        if(finished->value.sky != nullptr) {
            g_known = finished->value.sky;
            ready = std::move(finished->value);
        }
    }

    const auto now = std::chrono::steady_clock::now();

    if(!g_path.empty() && !g_pending.valid() && (g_requested || (watch && now - g_last_poll >= std::chrono::milliseconds { 500 }))) {
        g_requested = false;
        g_last_poll = now;

        g_pending = tw::plugin::bg_work::run<result>([path = g_path, previous = g_stamps, known = g_known] {
            return read(path, previous, known);
        });
    }

    return ready;
}
} // namespace tw::skybox::reload
