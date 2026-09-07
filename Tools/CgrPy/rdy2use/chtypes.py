"""Which DLL implements a channel type.

    uvrun chtypes                 # all 226, name -> dll
    uvrun chtypes array           # only types whose name contains "array"

Where it was used: finding `Array Value` -> `DF5BF7F7-...dll` and `Array Vector` ->
`DD626E09-...dll` before dumping their vtables, which is what settled whether a script can write a
table row at all (engine journal §2.2.3).
"""

import sys

from cgr.chtypes import entries


def main(pattern: str = ""):
    needle = pattern.lower()
    rows = [(name, dll) for name, dll in entries() if needle in name.lower()]

    width = max((len(name) for name, _ in rows), default=0)
    for name, dll in rows:
        print(f"{name:<{width}}  {dll}")

    print(f"\n{len(rows)} of {len(entries())} types", file=sys.stderr)


if __name__ == "__main__":
    main(*sys.argv[1:])
