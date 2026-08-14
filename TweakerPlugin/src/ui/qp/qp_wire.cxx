// No TweakerPlugin PCH here - this TU is shared with smoke_test (see src/ui/CMakeLists.txt) and
// has no D3D9/Quest3D dependency, only STL. Same convention as overlay_state.cxx.
#include "ui/qp/qp_wire.hxx"

#include "ui/overlay_state.hxx"
#include "ui/qp/qp_catalog.hxx"
#include "ui/qp/qp_state.hxx"
#include "ui/wire_text.hxx"

#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using tw::ui::qp::character_id;
using tw::ui::qp::tag_id;

namespace state = tw::ui::qp::state;

// The token every "no value here" field carries: a per-track character with no override, an entry
// id when the module has no position, an absent tag parameter. A bare empty token is impossible -
// L3 splits on spaces, so an empty field would simply vanish and shift every token after it.
constexpr std::string_view k_none_token = "-";

// A malformed count (a truncated message, a desync between the two sides) must not turn into a
// multi-gigabyte reserve. Chosen well above any real playlist rather than as a tight bound - the
// point is to reject nonsense, not to cap what the user is allowed to have.
constexpr int k_max_records = 1'000'000;

// Sequential token cursor over an op's argument list. The fixed-size split_fixed_tokens in
// overlay_ipc tops out at three tokens (the arity of TWEAK_SET) and the QP_* ops are all
// variable-arity, so they are read with a cursor and self-describing counts instead of a splitter -
// no intermediate array, no separator characters to escape.
class token_reader {
public:
    explicit token_reader(std::string_view text) noexcept : m_rest(text)
    {
        skip_spaces();
    }

    [[nodiscard]] bool exhausted() const noexcept
    {
        return m_rest.empty();
    }

    // Empty return means "nothing left" - which is also how every caller detects a truncated
    // message, since no valid field is ever empty (see k_none_token).
    std::string_view next() noexcept
    {
        if(m_rest.empty()) {
            return {};
        }

        const auto space = m_rest.find(' ');
        if(space == std::string_view::npos) {
            const std::string_view token = m_rest;
            m_rest = {};
            return token;
        }

        const std::string_view token = m_rest.substr(0, space);
        m_rest.remove_prefix(space + 1);
        skip_spaces();
        return token;
    }

    // Percent-decoded copy of the next token, for the fields that carry free text.
    std::string next_text()
    {
        return tw::ui::wire::percent_decode(next());
    }

    bool next_int(int& out) noexcept
    {
        const std::string_view token = next();
        if(token.empty()) {
            return false;
        }

        const auto* first = token.data();
        const auto* last = first + token.size();
        const auto result = std::from_chars(first, last, out);
        return result.ec == std::errc {} && result.ptr == last;
    }

private:
    void skip_spaces() noexcept
    {
        while(!m_rest.empty() && m_rest.front() == ' ') {
            m_rest.remove_prefix(1);
        }
    }

    std::string_view m_rest;
};

// Shared shape of every counted list in the QP_* ops: a count, then that many records. Returns
// false when the count itself is missing or absurd, so callers can bail before allocating.
bool read_count(token_reader& reader, int& out) noexcept
{
    return reader.next_int(out) && out >= 0 && out <= k_max_records;
}

void handle_catalog(token_reader& reader)
{
    int count = 0;
    if(!read_count(reader, count)) {
        return;
    }

    state::playlist_list playlists;
    for(int i = 0; i < count; ++i) {
        state::playlist_summary summary;
        summary.playlist_id = std::string(reader.next());
        summary.name = reader.next_text();

        if(summary.playlist_id.empty() || !reader.next_int(summary.track_count)) {
            // Truncated mid-record: publish nothing rather than a half catalog, which would read
            // to the user as "some of my playlists disappeared".
            return;
        }

        playlists.push_back(std::move(summary));
    }

    state::set_catalog(std::move(playlists));
}

// <entryId> <artist> <title> <character|-> <tagCount> [<tagWireName> <param|->] <modCount> [<modWireName>]
//
// Tag parameters are their own token rather than glued to the name: the alternative
// ("MinimumMatchSize:12") needs a separator that survives percent-encoding, and the whole reason
// the codec escapes everything outside the unreserved set is so callers never have to reason about
// which characters are structural.
bool read_track(token_reader& reader, state::track& out)
{
    out.entry_id = std::string(reader.next());
    if(out.entry_id.empty()) {
        return false;
    }

    out.artist = reader.next_text();
    out.title = reader.next_text();

    const std::string_view character = reader.next();
    if(character.empty()) {
        return false;
    }
    out.character = character == k_none_token ? character_id::none : tw::ui::qp::resolve_character_id(character);

    int tag_count = 0;
    if(!read_count(reader, tag_count)) {
        return false;
    }

    for(int i = 0; i < tag_count; ++i) {
        const std::string_view wire_name = reader.next();
        const std::string_view parameter = reader.next();
        if(wire_name.empty() || parameter.empty()) {
            return false;
        }

        state::tag_state tag;
        tag.id = tw::ui::qp::resolve_tag_id(wire_name);
        if(parameter != k_none_token) {
            const auto* first = parameter.data();
            const auto* last = first + parameter.size();
            tag.has_parameter = std::from_chars(first, last, tag.parameter).ec == std::errc {};
        }

        // An unknown tag is dropped rather than kept as unknown: it would render as an empty badge
        // and its toggle would address nothing. Dropping it keeps the rest of the track usable.
        if(tag.id != tag_id::unknown) {
            out.tags.push_back(tag);
        }
    }

    int mod_count = 0;
    if(!read_count(reader, mod_count)) {
        return false;
    }

    for(int i = 0; i < mod_count; ++i) {
        const std::string_view wire_name = reader.next();
        if(wire_name.empty()) {
            return false;
        }

        // Mods are the same seven tweaks the overlay already knows, so they resolve through
        // overlay_state rather than through a second catalog of their own.
        const auto id = tw::ui::overlay_state::resolve_tweak_id(wire_name);
        if(id != tw::ui::overlay_state::tweak_id::unknown) {
            out.mods.push_back(id);
        }
    }

    return true;
}

void handle_tracks(token_reader& reader)
{
    std::string playlist_id { reader.next() };
    if(playlist_id.empty()) {
        return;
    }

    int count = 0;
    if(!read_count(reader, count)) {
        return;
    }

    state::track_list tracks;
    for(int i = 0; i < count; ++i) {
        state::track entry;
        if(!read_track(reader, entry)) {
            return;
        }

        tracks.push_back(std::move(entry));
    }

    state::set_tracks(std::move(playlist_id), std::move(tracks));
}

void handle_order(token_reader& reader)
{
    state::order_state order;
    order.playlist_id = std::string(reader.next());
    if(order.playlist_id.empty()) {
        return;
    }

    const std::string_view mode = reader.next();
    const std::string_view advance = reader.next();
    const std::string_view current = reader.next();
    if(mode.empty() || advance.empty() || current.empty()) {
        return;
    }

    order.mode = tw::ui::qp::resolve_playback_mode(mode);
    order.advance = tw::ui::qp::resolve_advance_trigger(advance);
    if(current != k_none_token) {
        order.current_entry_id = current;
    }

    int count = 0;
    if(!read_count(reader, count)) {
        return;
    }

    for(int i = 0; i < count; ++i) {
        std::string entry_id { reader.next() };
        if(entry_id.empty()) {
            return;
        }

        order.upcoming.push_back(std::move(entry_id));
    }

    state::set_order(std::move(order));
}

void handle_now_playing(token_reader& reader)
{
    std::string playlist_id { reader.next() };
    std::string entry_id { reader.next() };
    if(playlist_id.empty() || entry_id.empty()) {
        return;
    }

    state::set_now_playing(std::move(playlist_id), std::move(entry_id));
}

void handle_default_character(token_reader& reader)
{
    const std::string_view character = reader.next();
    if(character.empty()) {
        return;
    }

    state::set_default_character(tw::ui::qp::resolve_character_id(character));
}

tw::ui::qp::send_fn g_send = nullptr;

bool send(const std::string& op_line)
{
    return g_send != nullptr && g_send(op_line);
}

// Ids and enum names are pure unreserved characters, so nothing outbound needs percent-encoding -
// the overlay never originates free text. Kept as its own helper anyway so a future op that does
// carry text cannot quietly skip the codec.
std::string with_id(std::string_view op, std::string_view id)
{
    std::string line { op };
    line += ' ';
    line.append(id);
    return line;
}
} // namespace

namespace tw::ui::qp
{
void handle_op(std::string_view op, std::string_view rest)
{
    token_reader reader(rest);

    if(op == "QP_CATALOG") {
        handle_catalog(reader);
        return;
    }

    if(op == "QP_TRACKS") {
        handle_tracks(reader);
        return;
    }

    if(op == "QP_ORDER") {
        handle_order(reader);
        return;
    }

    if(op == "QP_NOWPLAYING") {
        handle_now_playing(reader);
        return;
    }

    if(op == "QP_STOPPED") {
        state::set_stopped();
        return;
    }

    if(op == "QP_DEFAULT_CHARACTER") {
        handle_default_character(reader);
        return;
    }
}

void set_send_backend(send_fn fn) noexcept
{
    g_send = fn;
}

bool send_select(std::string_view playlist_id)
{
    return send(with_id("QP_NOTIFY_SELECT", playlist_id));
}

bool send_play(std::string_view playlist_id, std::string_view entry_id)
{
    std::string line = with_id("QP_NOTIFY_PLAY", playlist_id);
    line += ' ';
    line.append(entry_id);
    return send(line);
}

bool send_transport(std::string_view action)
{
    return send(with_id("QP_NOTIFY_TRANSPORT", action));
}

bool send_mode(std::string_view playlist_id, playback_mode mode)
{
    std::string line = with_id("QP_NOTIFY_MODE", playlist_id);
    line += ' ';
    line.append(playback_mode_wire_name(mode));
    return send(line);
}

bool send_advance(std::string_view playlist_id, advance_trigger trigger)
{
    std::string line = with_id("QP_NOTIFY_ADVANCE", playlist_id);
    line += ' ';
    line.append(advance_trigger_wire_name(trigger));
    return send(line);
}

bool send_default_character(character_id id)
{
    return send(with_id("QP_NOTIFY_DEFAULT_CHARACTER", character_wire_name(id)));
}

bool send_tag(std::string_view entry_id, tag_id id, bool enabled, int parameter, bool has_parameter)
{
    const std::string_view wire_name = tag(id).wire_name;
    if(wire_name.empty()) {
        return false;
    }

    std::string line = with_id("QP_NOTIFY_TAG", entry_id);
    line += ' ';
    line.append(wire_name);
    line += enabled ? " true" : " false";
    line += ' ';
    line += has_parameter ? std::to_string(parameter) : k_none_token;
    return send(line);
}

bool send_mod(std::string_view entry_id, overlay_state::tweak_id id, bool enabled)
{
    const std::string_view wire_name = overlay_state::tweak_wire_name(id);
    if(wire_name.empty()) {
        return false;
    }

    std::string line = with_id("QP_NOTIFY_MOD", entry_id);
    line += ' ';
    line.append(wire_name);
    line += enabled ? " true" : " false";
    return send(line);
}

bool send_character(std::string_view entry_id, character_id id)
{
    std::string line = with_id("QP_NOTIFY_CHARACTER", entry_id);
    line += ' ';
    line.append(id == character_id::none ? k_none_token : character_wire_name(id));
    return send(line);
}
} // namespace tw::ui::qp
