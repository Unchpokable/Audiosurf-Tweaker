#include "pch.hxx"

#include "resource/self_extract.hxx"

namespace
{
struct enum_context {
    HMODULE module = nullptr;
    tw::resource::type kind {};
    LPCWSTR type_name = nullptr;
    std::vector<tw::resource::self_extract::entry>* out = nullptr;
    bool ok = true;
};

bool wide_to_utf8(const wchar_t* wide, std::string& out)
{
    if(wide == nullptr) {
        return false;
    }

    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if(bytes <= 1) {
        return false;
    }

    out.resize(static_cast<std::size_t>(bytes - 1));
    const int written = WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), bytes, nullptr, nullptr);
    if(written <= 0) {
        return false;
    }

    // RC uppercases string names and may keep surrounding quotes in the PE.
    // Normalize to lowercase path keys matching assets/ relative paths.
    std::string normalized;
    normalized.reserve(out.size());
    for(char ch : out) {
        if(ch == '"') {
            continue;
        }
        if(ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
        if(ch == '\\') {
            ch = '/';
        }
        normalized.push_back(ch);
    }
    out = std::move(normalized);
    return !out.empty();
}

BOOL CALLBACK enum_names_proc(HMODULE module, LPCWSTR /*type*/, LPWSTR name, LONG_PTR param)
{
    auto* ctx = reinterpret_cast<enum_context*>(param);
    if(ctx == nullptr || ctx->out == nullptr || ctx->type_name == nullptr) {
        return FALSE;
    }

    // Integer resource IDs are not used by our generated .rc (string keys only).
    if(IS_INTRESOURCE(name)) {
        return TRUE;
    }

    std::string key;
    if(!wide_to_utf8(name, key)) {
        ctx->ok = false;
        return FALSE;
    }

    const HRSRC found = FindResourceW(module, name, ctx->type_name);
    if(found == nullptr) {
        ctx->ok = false;
        return FALSE;
    }

    const HGLOBAL loaded = LoadResource(module, found);
    if(loaded == nullptr) {
        ctx->ok = false;
        return FALSE;
    }

    void* locked = LockResource(loaded);
    const DWORD size = SizeofResource(module, found);
    if(locked == nullptr || size == 0) {
        ctx->ok = false;
        return FALSE;
    }

    tw::resource::self_extract::entry e;
    e.kind = ctx->kind;
    e.key = std::move(key);
    e.bytes = std::span<const std::byte>(static_cast<const std::byte*>(locked), static_cast<std::size_t>(size));
    ctx->out->push_back(std::move(e));
    return TRUE;
}

bool enumerate_type(HMODULE module, tw::resource::type kind, LPCWSTR type_name, std::vector<tw::resource::self_extract::entry>& out)
{
    enum_context ctx;
    ctx.module = module;
    ctx.kind = kind;
    ctx.type_name = type_name;
    ctx.out = &out;

    // EnumResourceNamesW returns FALSE when there are no resources of that type
    // OR on hard failure. Distinguish via GetLastError.
    SetLastError(ERROR_SUCCESS);
    const BOOL enumerated = EnumResourceNamesW(module, type_name, enum_names_proc, reinterpret_cast<LONG_PTR>(&ctx));
    if(!ctx.ok) {
        return false;
    }
    if(enumerated) {
        return true;
    }

    const DWORD err = GetLastError();
    // No resources of this type is fine (e.g. no .png / .txt yet).
    return err == ERROR_SUCCESS || err == ERROR_RESOURCE_TYPE_NOT_FOUND || err == ERROR_RESOURCE_DATA_NOT_FOUND
           || err == ERROR_RESOURCE_NAME_NOT_FOUND;
}
} // namespace

namespace tw::resource::self_extract
{
bool run(HMODULE module, std::vector<entry>& out) noexcept
{
    out.clear();
    if(module == nullptr) {
        return false;
    }

    if(!enumerate_type(module, type::texture, L"TW_TEXTURE", out)) {
        clear(out);
        return false;
    }
    if(!enumerate_type(module, type::vector, L"TW_SVG", out)) {
        clear(out);
        return false;
    }
    if(!enumerate_type(module, type::font, L"TW_FONT", out)) {
        clear(out);
        return false;
    }
    if(!enumerate_type(module, type::text, L"TW_TEXT", out)) {
        clear(out);
        return false;
    }
    return true;
}

void clear(std::vector<entry>& out) noexcept
{
    out.clear();
}
} // namespace tw::resource::self_extract
