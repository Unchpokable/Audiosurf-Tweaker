#include "shots.hxx"

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <GL/gl.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "libminiz/miniz.h"
#include "libstb/stb_image.h"

// resource.hxx uses std::span/std::expected without including them itself - see main.cxx.
#include "resource/resource.hxx"

#include "ui/fonts.hxx"
#include "ui/gpu_texture.hxx"
#include "ui/host_link.hxx"
#include "ui/image/svg.hxx"
#include "ui/overlay_state.hxx"
#include "ui/pending_actions.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/plugins/static/pins.hxx"
#include "ui/plugins/static/watermark.hxx"
#include "ui/qp/qp_pending.hxx"
#include "ui/qp/qp_state.hxx"
#include "ui/qp/qp_wire.hxx"
#include "ui/theme.hxx"

#include "fake_host.hxx"
#include "gl_texture.hxx"
#include "scene.hxx"

namespace
{
namespace fs = std::filesystem;

// One frame at 60 Hz - what the game runs at with vsync, and a value that does not divide into whole
// milliseconds, so the stand exercises detail::dt_ms()'s carry the way the game does.
constexpr float k_frame_dt = 1.f / 60.f;

// A scene that never calls finish() is a bug in the scene, not a long animation.
constexpr int k_max_frames = 3000;

// Same face and size the DLL bakes (framework/imgui_backend.cxx, load_font).
constexpr float k_font_px = 18.f;

// The interactive window's backdrop (main.cxx). The overlay has no background of its own in the game;
// this stands in for whatever the game draws behind it.
constexpr ImVec4 k_backdrop { 0x1B / 255.f, 0x1D / 255.f, 0x21 / 255.f, 1.f };

// ---------------------------------------------------------------------------------------------
// Offscreen target. The framebuffer-object entry points are GL 3.0 and not in opengl32's export table,
// so they are fetched by hand; the ImGui backend's own loader keeps its table private.
// ---------------------------------------------------------------------------------------------
constexpr GLenum k_gl_framebuffer = 0x8D40;
constexpr GLenum k_gl_renderbuffer = 0x8D41;
constexpr GLenum k_gl_rgba8 = 0x8058;
constexpr GLenum k_gl_color_attachment0 = 0x8CE0;
constexpr GLenum k_gl_framebuffer_complete = 0x8CD5;

using gen_names_fn = void(APIENTRY*)(GLsizei, GLuint*);
using delete_names_fn = void(APIENTRY*)(GLsizei, const GLuint*);
using bind_fn = void(APIENTRY*)(GLenum, GLuint);
using renderbuffer_storage_fn = void(APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei);
using framebuffer_renderbuffer_fn = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint);
using check_framebuffer_status_fn = GLenum(APIENTRY*)(GLenum);

struct fbo_api {
    gen_names_fn gen_framebuffers = nullptr;
    delete_names_fn delete_framebuffers = nullptr;
    bind_fn bind_framebuffer = nullptr;
    gen_names_fn gen_renderbuffers = nullptr;
    delete_names_fn delete_renderbuffers = nullptr;
    bind_fn bind_renderbuffer = nullptr;
    renderbuffer_storage_fn renderbuffer_storage = nullptr;
    framebuffer_renderbuffer_fn framebuffer_renderbuffer = nullptr;
    check_framebuffer_status_fn check_framebuffer_status = nullptr;
};

template<typename TFn>
bool load_gl(TFn& out, const char* name)
{
    out = reinterpret_cast<TFn>(wglGetProcAddress(name));
    return out != nullptr;
}

bool load_fbo_api(fbo_api& api)
{
    return load_gl(api.gen_framebuffers, "glGenFramebuffers") && load_gl(api.delete_framebuffers, "glDeleteFramebuffers")
           && load_gl(api.bind_framebuffer, "glBindFramebuffer") && load_gl(api.gen_renderbuffers, "glGenRenderbuffers")
           && load_gl(api.delete_renderbuffers, "glDeleteRenderbuffers") && load_gl(api.bind_renderbuffer, "glBindRenderbuffer")
           && load_gl(api.renderbuffer_storage, "glRenderbufferStorage")
           && load_gl(api.framebuffer_renderbuffer, "glFramebufferRenderbuffer")
           && load_gl(api.check_framebuffer_status, "glCheckFramebufferStatus");
}

// A window that is never shown, only there to own a GL context.
struct gl_host {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC context = nullptr;
};

constexpr const wchar_t* k_window_class = L"TweakerUiShots";

bool create_gl_host(gl_host& host)
{
    WNDCLASSEXW wc {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = ::DefWindowProcW;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = k_window_class;
    ::RegisterClassExW(&wc);

    host.hwnd = ::CreateWindowW(k_window_class, L"TweakerPlugin UI Shots", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    if(host.hwnd == nullptr) {
        return false;
    }

    host.hdc = ::GetDC(host.hwnd);
    PIXELFORMATDESCRIPTOR pfd {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int format = ::ChoosePixelFormat(host.hdc, &pfd);
    if(format == 0 || ::SetPixelFormat(host.hdc, format, &pfd) == FALSE) {
        return false;
    }

    host.context = wglCreateContext(host.hdc);
    return host.context != nullptr && wglMakeCurrent(host.hdc, host.context) != FALSE;
}

void destroy_gl_host(gl_host& host)
{
    wglMakeCurrent(nullptr, nullptr);
    if(host.context != nullptr) {
        wglDeleteContext(host.context);
    }
    if(host.hdc != nullptr) {
        ::ReleaseDC(host.hwnd, host.hdc);
    }
    if(host.hwnd != nullptr) {
        ::DestroyWindow(host.hwnd);
    }
    ::UnregisterClassW(k_window_class, ::GetModuleHandleW(nullptr));
}

struct offscreen {
    GLuint framebuffer = 0;
    GLuint color = 0;
    int width = 0;
    int height = 0;
};

bool create_offscreen(const fbo_api& api, offscreen& target, int width, int height)
{
    target.width = width;
    target.height = height;

    api.gen_renderbuffers(1, &target.color);
    api.bind_renderbuffer(k_gl_renderbuffer, target.color);
    api.renderbuffer_storage(k_gl_renderbuffer, k_gl_rgba8, width, height);
    api.bind_renderbuffer(k_gl_renderbuffer, 0);

    api.gen_framebuffers(1, &target.framebuffer);
    api.bind_framebuffer(k_gl_framebuffer, target.framebuffer);
    api.framebuffer_renderbuffer(k_gl_framebuffer, k_gl_color_attachment0, k_gl_renderbuffer, target.color);
    const bool complete = api.check_framebuffer_status(k_gl_framebuffer) == k_gl_framebuffer_complete;
    api.bind_framebuffer(k_gl_framebuffer, 0);

    return complete;
}

void destroy_offscreen(const fbo_api& api, offscreen& target)
{
    if(target.framebuffer != 0) {
        api.delete_framebuffers(1, &target.framebuffer);
    }
    if(target.color != 0) {
        api.delete_renderbuffers(1, &target.color);
    }
    target = {};
}

// ---------------------------------------------------------------------------------------------
// PNG in and out. Written by miniz's PNG encoder, read by stb_image - both already in the tree.
// RGB, no alpha: the backdrop is opaque, so alpha carries nothing, and leaving it out keeps a diff
// from flagging blending noise nobody can see.
// ---------------------------------------------------------------------------------------------
bool write_file(const fs::path& path, const void* data, std::size_t size)
{
    std::ofstream file { path, std::ios::binary | std::ios::trunc };
    file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return file.good();
}

// `bottom_up` because glReadPixels returns rows from the bottom of the framebuffer.
bool write_png(const fs::path& path, const unsigned char* rgb, int width, int height, bool bottom_up)
{
    std::size_t size = 0;
    void* png = tdefl_write_image_to_png_file_in_memory_ex(rgb, width, height, 3, &size, 6, bottom_up ? MZ_TRUE : MZ_FALSE);
    if(png == nullptr) {
        return false;
    }

    const bool ok = write_file(path, png, size);
    mz_free(png);
    return ok;
}

struct image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgb;
};

std::expected<image, std::string> read_png(const fs::path& path)
{
    std::ifstream file { path, std::ios::binary };
    if(!file) {
        return std::unexpected("cannot open");
    }
    const std::vector<unsigned char> bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 3);
    if(pixels == nullptr) {
        return std::unexpected(stbi_failure_reason() != nullptr ? stbi_failure_reason() : "decode failed");
    }

    image out { width, height, std::vector<unsigned char>(pixels, pixels + static_cast<std::size_t>(width) * height * 3) };
    stbi_image_free(pixels);
    return out;
}

std::string narrow(const std::wstring& text)
{
    return fs::path(text).string();
}

// ---------------------------------------------------------------------------------------------
// One scene, in this process.
// ---------------------------------------------------------------------------------------------
const tw::smoke::scene* find_scene(std::string_view name)
{
    for(const tw::smoke::scene& candidate : tw::smoke::all_scenes()) {
        if(name == candidate.name) {
            return &candidate;
        }
    }
    return nullptr;
}

fs::path shot_path(const fs::path& dir, std::string_view scene, std::string_view shot)
{
    std::string file { scene };
    if(!shot.empty()) {
        file += '-';
        file += shot;
    }
    file += ".png";
    return dir / file;
}

int run_scene(const tw::smoke::scene& scene, const fs::path& out_dir)
{
    if(!tw::resource::initialize(::GetModuleHandleW(nullptr))) {
        std::fprintf(stderr, "%s: resource::initialize failed\n", scene.name);
        return 2;
    }

    // The same wiring as the interactive window, minus overlay_config::load(): the stand runs on the
    // defaults, so a smoke_overlay.cfg left over from an interactive session cannot move the menu or
    // recolour the theme between two runs. Without a loaded path, overlay_config::flush() writes nothing.
    tw::ui::gpu_texture::set_backend(&tw::smoke::gl_upload_texture, &tw::smoke::gl_release_texture);
    tw::ui::image::svg::initialize();
    tw::ui::pending_actions::set_send_backend(&tw::smoke::send_overlay_command);
    tw::ui::qp::set_send_backend(&tw::smoke::send_overlay_command);
    tw::smoke::seed_fake_overlay_state();
    tw::smoke::seed_fake_quick_player();
    tw::ui::theme::apply_dark();

    gl_host host;
    fbo_api api;
    offscreen target;
    const int width = static_cast<int>(scene.canvas.x);
    const int height = static_cast<int>(scene.canvas.y);

    if(!create_gl_host(host) || !load_fbo_api(api) || !create_offscreen(api, target, width, height)) {
        std::fprintf(stderr, "%s: no OpenGL context or framebuffer object\n", scene.name);
        destroy_gl_host(host);
        return 2;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = scene.canvas;
    io.DisplayFramebufferScale = ImVec2 { 1.f, 1.f };

    if(!tw::ui::fonts::load(io.Fonts, k_font_px)) {
        std::fprintf(stderr, "%s: fonts::load failed\n", scene.name);
        ImGui::DestroyContext();
        destroy_offscreen(api, target);
        destroy_gl_host(host);
        return 2;
    }

    // Exactly what the DLL does (framework/imgui_backend.cxx): the theme's style, no DPI scaling.
    tw::ui::theme::apply_to(ImGui::GetStyle());
    ImGui_ImplOpenGL3_Init();

    tw::ui::plugins::statics::watermark::initialize();
    tw::ui::plugins::statics::pins::initialize();
    tw::ui::plugins::statics::notefeed::initialize();
    tw::ui::plugins::interactive::menu::initialize();

    tw::ui::overlay_state::cache overlay_cache;
    tw::ui::qp::state::cache qp_cache;
    tw::smoke::scene_ctx ctx { scene.canvas, overlay_cache, qp_cache };

    if(scene.setup != nullptr) {
        scene.setup(ctx);
    }

    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    int status = 1;

    for(int frame = 0; frame < k_max_frames; ++frame) {
        io.DeltaTime = k_frame_dt;
        io.DisplaySize = scene.canvas;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
        tw::ui::theme::sync_colors(ImGui::GetStyle());

        // The per-frame sequence of ui_main::draw_frame and the interactive loop.
        tw::ui::image::svg::update();
        tw::ui::overlay_state::refresh(overlay_cache);
        (void)tw::ui::host_link::update(overlay_cache);
        tw::ui::pending_actions::update(overlay_cache);
        tw::ui::qp::state::refresh(qp_cache);
        tw::ui::qp::pending::update(qp_cache);

        ctx.begin_frame(frame);
        scene.frame(ctx);

        // The menu turns on ImGui's software cursor while it is open. In a snapshot that is an arrow
        // painted over whatever the scene is hovering, so it goes.
        io.MouseDrawCursor = false;

        ImGui::Render();

        api.bind_framebuffer(k_gl_framebuffer, target.framebuffer);
        glViewport(0, 0, width, height);
        glClearColor(k_backdrop.x, k_backdrop.y, k_backdrop.z, k_backdrop.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        bool written = true;
        for(const std::string& shot : ctx.take_captures()) {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

            const fs::path path = shot_path(out_dir, scene.name, shot);
            if(!write_png(path, pixels.data(), width, height, true)) {
                std::fprintf(stderr, "%s: cannot write %s\n", scene.name, path.string().c_str());
                written = false;
            }
        }
        api.bind_framebuffer(k_gl_framebuffer, 0);

        if(!written) {
            status = 2;
            break;
        }
        if(ctx.finished()) {
            status = 0;
            break;
        }
    }

    if(status == 1) {
        std::fprintf(stderr, "%s: did not finish within %d frames\n", scene.name, k_max_frames);
    }

    tw::ui::plugins::interactive::menu::shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    destroy_offscreen(api, target);
    destroy_gl_host(host);
    return status;
}

// ---------------------------------------------------------------------------------------------
// Every scene, each in a process of its own. The overlay modules keep their state in file-level
// globals (menu, player) that nothing resets, so a scene run after another would inherit its tab, its
// hover animation and its open popups. A fresh process is the only clean slate there is.
// ---------------------------------------------------------------------------------------------
int spawn_scene(const std::wstring& exe, std::string_view name, const fs::path& out_dir)
{
    std::wstring command = L"\"" + exe + L"\" --scene " + fs::path(std::string { name }).wstring() + L" --out \"" + out_dir.wstring() + L"\"";

    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    if(::CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process) == FALSE) {
        return 2;
    }

    ::WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 2;
    ::GetExitCodeProcess(process.hProcess, &code);
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    return static_cast<int>(code);
}

int run_all(const fs::path& out_dir, std::string_view filter)
{
    std::error_code error;
    fs::create_directories(out_dir, error);
    if(error) {
        std::fprintf(stderr, "cannot create %s\n", out_dir.string().c_str());
        return 2;
    }

    wchar_t exe[MAX_PATH] {};
    ::GetModuleFileNameW(nullptr, exe, MAX_PATH);

    int ran = 0;
    int failed = 0;
    for(const tw::smoke::scene& scene : tw::smoke::all_scenes()) {
        if(!filter.empty() && std::string_view { scene.name }.find(filter) == std::string_view::npos) {
            continue;
        }

        ++ran;
        const int code = spawn_scene(exe, scene.name, fs::absolute(out_dir));
        std::printf("%-28s %s\n", scene.name, code == 0 ? "ok" : "FAILED");
        if(code != 0) {
            ++failed;
        }
    }

    std::printf("%d scene(s), %d failed -> %s\n", ran, failed, fs::absolute(out_dir).string().c_str());
    if(ran == 0) {
        return 2;
    }
    return failed == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------
// Comparison.
// ---------------------------------------------------------------------------------------------
std::set<std::string> png_names(const fs::path& dir)
{
    std::set<std::string> names;
    std::error_code error;
    for(const fs::directory_entry& entry : fs::directory_iterator(dir, error)) {
        if(entry.is_regular_file() && entry.path().extension() == ".png") {
            names.insert(entry.path().filename().string());
        }
    }
    return names;
}

// The difference image: identical pixels as a dim copy of the first image, so the layout stays
// recognisable, and every differing pixel in full magenta.
void write_diff(const fs::path& path, const image& a, const image& b)
{
    std::vector<unsigned char> out(a.rgb.size());
    for(std::size_t i = 0; i < a.rgb.size(); i += 3) {
        const bool same = a.rgb[i] == b.rgb[i] && a.rgb[i + 1] == b.rgb[i + 1] && a.rgb[i + 2] == b.rgb[i + 2];
        if(same) {
            out[i] = static_cast<unsigned char>(a.rgb[i] / 4);
            out[i + 1] = static_cast<unsigned char>(a.rgb[i + 1] / 4);
            out[i + 2] = static_cast<unsigned char>(a.rgb[i + 2] / 4);
        }
        else {
            out[i] = 255;
            out[i + 1] = 0;
            out[i + 2] = 255;
        }
    }
    (void)write_png(path, out.data(), a.width, a.height, false);
}

int compare(const fs::path& a_dir, const fs::path& b_dir, const fs::path& diff_dir)
{
    if(!fs::is_directory(a_dir) || !fs::is_directory(b_dir)) {
        std::fprintf(stderr, "both arguments must be directories\n");
        return 2;
    }
    if(!diff_dir.empty()) {
        std::error_code error;
        fs::create_directories(diff_dir, error);
    }

    const std::set<std::string> a_names = png_names(a_dir);
    const std::set<std::string> b_names = png_names(b_dir);
    std::set<std::string> all = a_names;
    all.insert(b_names.begin(), b_names.end());

    int same = 0;
    int different = 0;
    int missing = 0;
    for(const std::string& name : all) {
        if(!a_names.contains(name) || !b_names.contains(name)) {
            std::printf("%-40s only in %s\n", name.c_str(), a_names.contains(name) ? "a" : "b");
            ++missing;
            continue;
        }

        const auto a = read_png(a_dir / name);
        const auto b = read_png(b_dir / name);
        if(!a || !b) {
            std::fprintf(stderr, "%s: %s\n", name.c_str(), (!a ? a.error() : b.error()).c_str());
            return 2;
        }

        if(a->width != b->width || a->height != b->height) {
            std::printf("%-40s size %dx%d vs %dx%d\n", name.c_str(), a->width, a->height, b->width, b->height);
            ++different;
            continue;
        }

        int count = 0;
        int x0 = a->width;
        int y0 = a->height;
        int x1 = -1;
        int y1 = -1;
        for(int y = 0; y < a->height; ++y) {
            for(int x = 0; x < a->width; ++x) {
                const std::size_t i = (static_cast<std::size_t>(y) * a->width + x) * 3;
                if(std::memcmp(&a->rgb[i], &b->rgb[i], 3) != 0) {
                    ++count;
                    x0 = (std::min)(x0, x);
                    y0 = (std::min)(y0, y);
                    x1 = (std::max)(x1, x);
                    y1 = (std::max)(y1, y);
                }
            }
        }

        if(count == 0) {
            ++same;
            continue;
        }

        ++different;
        std::printf("%-40s %d px differ in [%d,%d]-[%d,%d]\n", name.c_str(), count, x0, y0, x1, y1);
        if(!diff_dir.empty()) {
            write_diff(diff_dir / name, *a, *b);
        }
    }

    std::printf("%d identical, %d different, %d unmatched\n", same, different, missing);
    return different == 0 && missing == 0 ? 0 : 1;
}

void print_usage()
{
    std::fprintf(stderr,
        "usage:\n"
        "  smoke_test                                  interactive window\n"
        "  smoke_test --list\n"
        "  smoke_test --shots <dir> [--filter <s>]\n"
        "  smoke_test --scene <name> --out <dir>\n"
        "  smoke_test --compare <a> <b> [--diff <dir>]\n");
}

// The value after `flag`, or empty.
std::wstring option(const std::vector<std::wstring>& args, std::wstring_view flag)
{
    for(std::size_t i = 0; i + 1 < args.size(); ++i) {
        if(args[i] == flag) {
            return args[i + 1];
        }
    }
    return {};
}
} // namespace

namespace tw::smoke::shots
{
int run(const std::vector<std::wstring>& args)
{
    const std::wstring& command = args.front();

    if(command == L"--list") {
        for(const scene& candidate : all_scenes()) {
            std::printf("%-28s %s\n", candidate.name, candidate.description);
        }
        return 0;
    }

    if(command == L"--shots" && args.size() >= 2) {
        return run_all(fs::path { args[1] }, narrow(option(args, L"--filter")));
    }

    if(command == L"--scene" && args.size() >= 2) {
        const std::string name = narrow(args[1]);
        const scene* found = find_scene(name);
        const std::wstring out = option(args, L"--out");
        if(found == nullptr || out.empty()) {
            std::fprintf(stderr, "unknown scene '%s' or no --out\n", name.c_str());
            return 2;
        }
        return run_scene(*found, fs::path { out });
    }

    if(command == L"--compare" && args.size() >= 3) {
        return compare(fs::path { args[1] }, fs::path { args[2] }, fs::path { option(args, L"--diff") });
    }

    print_usage();
    return 2;
}
} // namespace tw::smoke::shots
