-- ---------------------------------------------------------------------------------------------------
-- tw.ui - a script's own tabs in the overlay menu, and the controls on them.
--
-- A script declares its tab once - while it loads, or from tw.on_ready - and keeps the handles. It
-- never draws the tab: C does, every frame the tab is open, from a model it owns (lua_tabs, ui/form),
-- without entering the VM. The script reads that model (ctl:get()), writes it (ctl:set(v)), and hears
-- about what the user did through ctl:on_change(fn) - fn(value, committed), run under the same guard
-- and budget as every other callback, by exports.dispatch_ui, which C calls once a frame in `ready`
-- when the queue is not empty. Docs/Internal/script-tabs.md §3.5-§3.6.
--
-- A handle is a table holding the node's number in C. One that was refused - declared outside the
-- registration window, or past a limit - or whose script has since been unloaded is inert: get()
-- answers the initial value, everything else does nothing. Never an error for the script: a settings
-- tab with one bad line is still a settings tab.
--
-- **The user wins.** set() never raises on_change, does nothing when the value is the same, and loses
-- against a gesture in progress - a slider being dragged ignores it, and the release reports the
-- user's number.
-- ---------------------------------------------------------------------------------------------------

local ffi = require("ffi")
local S = ...
local C = S.C
local tw = S.tw
local caller_owner, guarded, window_open, may_register = S.caller_owner, S.guarded, S.window_open, S.may_register
local K_UI, D_WARN = S.K_UI, S.D_WARN
local exports = S.exports

local C_tab, C_add, C_range, C_options = C.tw_ui_tab, C.tw_ui_add, C.tw_ui_range, C.tw_ui_options
local C_items, C_height, C_get, C_set = C.tw_ui_items, C.tw_ui_height, C.tw_ui_get, C.tw_ui_set
local C_text, C_flags, C_tab_visible, C_next_event = C.tw_ui_text, C.tw_ui_flags, C.tw_ui_tab_visible, C.tw_ui_next_event
local C_diag = C.tw_diag

-- Kinds of node, same numbers as tw::ui::form::kind.
local GROUP, TOGGLE, SLIDER, NUMBER, CHOICE, COLOR, BUTTON, TEXT, SEPARATOR, LIST = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9

-- What tw_ui_text sets, same numbers as tw::lua::tabs::text_slot.
local T_LABEL, T_TOOLTIP = 0, 1

local ev_node, ev_value, ev_committed = ffi.new("int[1]"), ffi.new("double[1]"), ffi.new("int[1]")

-- The handlers, by node number: { owner = , ctl = , fn, fn, ... }. Numbers carry a generation, so a
-- node of a reloaded script never finds the old script's handlers.
local ui_handlers = {}

-- A Lua array of strings as a C array for one call. The strings stay referenced by `list` for as long
-- as the caller holds it, which is the whole call.
local function c_strings(list, n)
    local arr = ffi.new("const char*[?]", n > 0 and n or 1)
    for i = 1, n do arr[i - 1] = list[i] end
    return arr
end

-- The argument table of a constructor; a bare string is its label. Level 3 is the script: 1 is this,
-- 2 the method, 3 its caller - so these are called straight from the methods, never through a helper.
local function options_of(what, opts)
    if opts == nil then return {} end
    if type(opts) == "string" then return { label = opts } end
    if type(opts) ~= "table" then error(what .. " expects a table of options", 3) end
    return opts
end

local function number_of(what, opts, field, default)
    local v = opts[field]
    if v == nil then return default end
    if type(v) ~= "number" then error(what .. ": " .. field .. " must be a number", 3) end
    return v
end

local function text_of(v)
    if v == nil then return "" end
    return tostring(v)
end

-- What every node can do.
local Base = {}
function Base:enable(on) C_flags(self.id, on == false and 0 or 1, -1) return self end
function Base:show(on) C_flags(self.id, -1, on == false and 0 or 1) return self end
function Base:label(text) C_text(self.id, T_LABEL, text_of(text)) return self end
function Base:tooltip(text) C_text(self.id, T_TOOLTIP, text_of(text)) return self end

-- One table per handle type, methods copied in rather than chained: a lookup is one hop, and the
-- types read as flat lists in the API description.
local function handle_type(...)
    local t = {}
    t.__index = t
    for _, mixin in ipairs({ ... }) do
        for k, v in pairs(mixin) do t[k] = v end
    end
    return t
end

local function subscribe(self, fn)
    if self.id < 0 then return end
    local rec = ui_handlers[self.id]
    if rec == nil then
        rec = { owner = self.owner, ctl = self }
        ui_handlers[self.id] = rec
    end
    rec[#rec + 1] = fn
end

-- toggle, slider, number, colour.
local Control = handle_type(Base)

function Control:get()
    local v = C_get(self.id)
    if v ~= v then return self.initial end
    if self.kind == TOGGLE then return v ~= 0 end
    return v
end

function Control:set(value)
    if self.kind == TOGGLE then
        C_set(self.id, value and 1 or 0)
    else
        if type(value) ~= "number" then error("set expects a number", 2) end
        C_set(self.id, value)
    end
    return self
end

function Control:on_change(fn)
    if may_register(self.owner, "on_change", fn) then subscribe(self, fn) end
    return self
end

function Control:_from(v)
    if self.kind == TOGGLE then return v ~= 0 end
    return v
end

-- A choice answers with its option table, { text = , index = }: one per option, built when the
-- options are, and the same table every call - get() in on_frame makes no garbage. Indices from 1.
local Choice = handle_type(Base)
Choice.on_change = Control.on_change

local function build_choices(self, labels)
    local list, texts = {}, {}
    for i = 1, #labels do
        texts[i] = tostring(labels[i])
        list[i] = { text = texts[i], index = i }
    end
    self.choices = list
    C_options(self.id, c_strings(texts, #texts), #texts)
end

-- An option by its text or its number; nil, with a word to the author, for one that is not there - a
-- value read back from a settings file can name an option a newer version of the script dropped.
local function find_choice(self, v)
    local found
    if type(v) == "number" then
        found = self.choices[v]
    else
        local text = tostring(v)
        for i = 1, #self.choices do
            if self.choices[i].text == text then found = self.choices[i] break end
        end
    end
    if found == nil then
        C_diag(self.owner, D_WARN, "tw.ui", "a choice has no option " .. tostring(v) .. " - left as it was")
    end
    return found
end

function Choice:get()
    local v = C_get(self.id)
    if v ~= v then return self.initial end
    return self.choices[v + 1]
end

function Choice:set(option)
    local found = find_choice(self, option)
    if found ~= nil then C_set(self.id, found.index - 1) end
    return self
end

function Choice:options(labels)
    if type(labels) ~= "table" then error("options expects a table of strings", 2) end
    build_choices(self, labels)
    return self
end

function Choice:_from(v) return self.choices[v + 1] end

-- A list answers with the selected row's number, from 1, or nil.
local List = handle_type(Base)
List.on_change = Control.on_change

local function push_rows(self, rows)
    local texts, subs, any_sub = {}, {}, false
    for i = 1, #rows do
        local row = rows[i]
        if type(row) == "table" then
            texts[i], subs[i] = text_of(row.text), text_of(row.sub)
            any_sub = any_sub or subs[i] ~= ""
        else
            texts[i], subs[i] = text_of(row), ""
        end
    end
    C_items(self.id, c_strings(texts, #texts), any_sub and c_strings(subs, #subs) or nil, #texts)
end

function List:get()
    local v = C_get(self.id)
    if v ~= v then return self.initial end
    if v < 0 then return nil end
    return v + 1
end

function List:set(row)
    if row ~= nil and type(row) ~= "number" then error("set expects a row number or nil", 2) end
    C_set(self.id, row ~= nil and row - 1 or -1)
    return self
end

function List:items(rows)
    if type(rows) ~= "table" then error("items expects a table of rows", 2) end
    push_rows(self, rows)
    return self
end

function List:_from(v)
    if v < 0 then return nil end
    return v + 1
end

-- A button has no value; a click is on_click(fn), fn() with no arguments.
local Button = handle_type(Base)

function Button:on_click(fn)
    if may_register(self.owner, "on_click", fn) then subscribe(self, fn) end
    return self
end

function Button:_from() return nil end

-- A paragraph of text; set() replaces it.
local Text = handle_type(Base)
Text.set = Base.label

local Separator = { show = Base.show }
Separator.__index = Separator

-- Where controls go: a tab, or a group on it.
local Container = {}

-- A node on `parent`, inert unless the window is open and C took it. `what` names the call in the
-- diagnostic a refusal leaves.
local function node(parent, what, kind, label, mt)
    local h = setmetatable({ id = -1, owner = parent.owner, kind = kind }, mt)
    if parent.id < 0 or not window_open(parent.owner, what) then return h end
    h.id = C_add(parent.id, kind, label)
    return h
end

-- tooltip, enabled and visible, which every constructor takes.
local function finish(h, opts)
    if h.id >= 0 then
        if opts.tooltip ~= nil then C_text(h.id, T_TOOLTIP, text_of(opts.tooltip)) end
        if opts.enabled == false then C_flags(h.id, 0, -1) end
        if opts.visible == false then C_flags(h.id, -1, 0) end
    end
    return h
end

local Group

function Container:group(opts)
    opts = options_of("group", opts)
    return finish(node(self, "group", GROUP, text_of(opts.label), Group), opts)
end

function Container:toggle(opts)
    opts = options_of("toggle", opts)
    local h = node(self, "toggle", TOGGLE, text_of(opts.label), Control)
    h.initial = opts.value and true or false
    C_set(h.id, h.initial and 1 or 0)
    return finish(h, opts)
end

-- slider and number: the same shape, a range and a value in it. The initial value is what C made of
-- it - clamped, and rounded for a number - so get() on an inert handle agrees with a live one.
local function ranged(self, opts, what, kind, lo, hi)
    local h = node(self, what, kind, text_of(opts.label), Control)
    C_range(h.id, lo, hi)
    C_set(h.id, opts.value ~= nil and opts.value or lo)
    local v = C_get(h.id)
    h.initial = v == v and v or (opts.value or lo)
    return finish(h, opts)
end

function Container:slider(opts)
    opts = options_of("slider", opts)
    local lo, hi = number_of("slider", opts, "min", 0), number_of("slider", opts, "max", 1)
    if opts.value ~= nil and type(opts.value) ~= "number" then error("slider: value must be a number", 2) end
    return ranged(self, opts, "slider", SLIDER, lo, hi)
end

function Container:number(opts)
    opts = options_of("number", opts)
    local lo, hi = number_of("number", opts, "min", 0), number_of("number", opts, "max", 100)
    if opts.value ~= nil and type(opts.value) ~= "number" then error("number: value must be a number", 2) end
    return ranged(self, opts, "number", NUMBER, lo, hi)
end

-- `options` is required; `value` is an option's text or number, the first one if absent.
function Container:choice(opts)
    opts = options_of("choice", opts)
    if type(opts.options) ~= "table" then error("choice: options must be a table of strings", 2) end
    local h = node(self, "choice", CHOICE, text_of(opts.label), Choice)
    build_choices(h, opts.options)
    local first = h.choices[1]
    local initial = opts.value ~= nil and find_choice(h, opts.value) or first
    if initial ~= nil then C_set(h.id, initial.index - 1) end
    h.initial = initial
    return finish(h, opts)
end

-- A colour packed the way tw.rgb, tw.theme and every tw.hud.* call take it; white if absent.
function Container:color(opts)
    opts = options_of("color", opts)
    local v = number_of("color", opts, "value", 0xFFFFFFFF)
    local h = node(self, "color", COLOR, text_of(opts.label), Control)
    C_set(h.id, v)
    h.initial = v
    return finish(h, opts)
end

function Container:button(opts)
    opts = options_of("button", opts)
    return finish(node(self, "button", BUTTON, text_of(opts.label), Button), opts)
end

-- tab:text("...") or tab:text{ text = "...", tooltip = ... }.
function Container:text(opts)
    if type(opts) == "string" or type(opts) == "number" then opts = { text = opts } end
    opts = options_of("text", opts)
    return finish(node(self, "text", TEXT, text_of(opts.text), Text), opts)
end

function Container:separator()
    return node(self, "separator", SEPARATOR, "", Separator)
end

-- `items` - strings, or { text = , sub = } for a row with a second line; `value` - the selected row,
-- from 1, or none; `height` in pixels.
function Container:list(opts)
    opts = options_of("list", opts)
    if opts.items ~= nil and type(opts.items) ~= "table" then error("list: items must be a table of rows", 2) end
    if opts.value ~= nil and type(opts.value) ~= "number" then error("list: value must be a row number", 2) end
    local h = node(self, "list", LIST, text_of(opts.label), List)
    if opts.items ~= nil then push_rows(h, opts.items) end
    if opts.height ~= nil then C_height(h.id, number_of("list", opts, "height", 160)) end
    C_set(h.id, opts.value ~= nil and opts.value - 1 or -1)
    h.initial = opts.value
    return finish(h, opts)
end

Group = handle_type(Base, Container)

local Tab = handle_type(Container)
function Tab:label(text) C_text(self.id, T_LABEL, text_of(text)) return self end

-- Whether the tab was on screen this frame or the last: no point building a long status text for a
-- tab nobody is looking at.
function Tab:visible() return C_tab_visible(self.id) ~= 0 end

tw.ui = {}

-- A new tab in the overlay menu, after the built-in ones, in the user's order of scripts and then in
-- the order the script declares them. tw.ui.tab{ label = "..." } or tw.ui.tab("..."); no label names
-- it after the script. A label that clashes with another tab's is shown with the script's name in
-- front.
function tw.ui.tab(opts)
    local owner = caller_owner()
    opts = options_of("tw.ui.tab", opts)
    local h = setmetatable({ id = -1, owner = owner }, Tab)
    if not window_open(owner, "tw.ui.tab") then return h end
    h.id = C_tab(owner, text_of(opts.label))
    return h
end

-- For tooling, like the handle types of tw.channels - whose names these must not take: TextHandle is a
-- string channel's, which is why a paragraph's is ParagraphHandle. A name taken twice stops the prelude.
local ui_types = {
    TabHandle = Tab, GroupHandle = Group, ControlHandle = Control, ChoiceHandle = Choice, ListHandle = List,
    ButtonHandle = Button, ParagraphHandle = Text, SeparatorHandle = Separator,
}
for name, t in pairs(ui_types) do
    if S.handle_types[name] ~= nil then error("prelude: handle type " .. name .. " is defined twice", 0) end
    S.handle_types[name] = t
end
S.constructors["ui.tab"] = "TabHandle"

-- What each method hands back, for the API description (the editor's tw.def.lua types the result of
-- tab:toggle{} as a control) - the methods' counterpart of S.constructors. Constructors hand back the
-- new node; everything else hands back the handle it was called on, so calls chain.
local CONSTRUCTED = {
    group = "GroupHandle", toggle = "ControlHandle", slider = "ControlHandle", number = "ControlHandle",
    choice = "ChoiceHandle", color = "ControlHandle", button = "ButtonHandle", text = "ParagraphHandle",
    separator = "SeparatorHandle", list = "ListHandle",
}
local CHAINED = { "enable", "show", "label", "tooltip", "set", "on_change", "on_click", "options", "items" }
S.method_returns = S.method_returns or {}
for name, t in pairs(ui_types) do
    for method, returns in pairs(CONSTRUCTED) do
        if rawget(t, method) ~= nil then S.method_returns[name .. ":" .. method] = returns end
    end
    for _, method in ipairs(CHAINED) do
        if rawget(t, method) ~= nil then S.method_returns[name .. ":" .. method] = name end
    end
end

-- What the user did since the last call, one entry per control, in the order they first changed. C
-- calls this only in `ready`, and only when there is something to deliver.
function exports.dispatch_ui()
    while C_next_event(ev_node, ev_value, ev_committed) ~= 0 do
        local rec = ui_handlers[ev_node[0]]
        if rec ~= nil then
            local value = rec.ctl:_from(ev_value[0])
            local committed = ev_committed[0] ~= 0
            for i = 1, #rec do
                guarded(rec.owner, K_UI, rec[i], value, committed)
            end
        end
    end
end

-- A script going away loses its handlers here; its nodes go inert in C (lua_tabs::drop_owner), after its
-- on_unload has had the chance to read them.
local unload_owner = exports.unload_owner
function exports.unload_owner(id)
    unload_owner(id)
    for n, rec in pairs(ui_handlers) do
        if rec.owner == id then ui_handlers[n] = nil end
    end
end
