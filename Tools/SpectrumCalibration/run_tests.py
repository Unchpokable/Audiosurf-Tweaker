"""Run test_reader.lua on LuaJIT 2.1 - the same VM the plugin embeds.

    uv run --with lupa python run_tests.py

Only a launcher. The test is Lua because the thing under test is Lua, and it has to run on a VM
whose standard library matches the plugin's: `minilua` (the one binary the LuaJIT build already
produces) is stripped down to what DynASM needs and has neither `math` nor `tostring`, so it cannot
load the script under test at all, let alone exercise it. lupa ships real LuaJIT 2.1 and needs no
build step.
"""
import sys
from pathlib import Path

from lupa.luajit21 import LuaRuntime

HERE = Path(__file__).resolve().parent


def main():
    lua = LuaRuntime(unpack_returned_tuples=True)

    # The test resolves the script under test relative to the working directory.
    source = (HERE / "test_reader.lua").read_text(encoding="utf-8")
    chunk = lua.eval("function(src, name) return assert(loadstring(src, name)) end")(source, "@test_reader.lua")

    failures = chunk()
    return int(failures or 0)


if __name__ == "__main__":
    import os

    os.chdir(HERE)
    sys.exit(1 if main() else 0)
