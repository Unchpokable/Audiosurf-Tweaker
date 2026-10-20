-- ---------------------------------------------------------------------------------------------------
-- tw.sandbox - what a script cannot reach. Last, because every section above has already taken what
-- it needs from these names into locals: ffi for the casts, debug for tracebacks and caller places.
--
-- It used to be C (lua_vm.cxx, strip_sandbox), with a second copy in harness/lua/ljtest that had
-- already drifted - it never removed the os fields. As prelude text there is one list, the harness
-- runs it because it runs the prelude, and what it removes is also what harness/lua writes into
-- Docs/scripting/api.json, which is how twllink knows to reject `io.open` at build time instead of a
-- script finding out at runtime (lua-engine-fix-roadmap.md §9.8, Ф5b).
--
-- Why each name goes is in Docs/scripting/limits.md, "What is not in the environment". The list is
-- not a security boundary and does not pretend to be one - it keeps honest mistakes survivable.
-- ---------------------------------------------------------------------------------------------------

local S = ...

local GLOBALS = {
    "ffi", "io", "package", "require", "dofile", "loadfile", "load", "loadstring", "debug", "jit",
    "newproxy", "collectgarbage",
}
local OS_FIELDS = { "execute", "remove", "rename", "tmpname", "exit", "getenv", "setlocale" }

for _, name in ipairs(GLOBALS) do
    _G[name] = nil
end
for _, name in ipairs(OS_FIELDS) do
    os[name] = nil
end

-- For tooling only (harness/lua reads it to describe the environment); nothing at runtime looks.
S.sandbox = { globals = GLOBALS, os = OS_FIELDS }
