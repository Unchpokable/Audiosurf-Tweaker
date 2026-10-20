"""What `cgrpy` runs: finds the tool you named and runs it as a script, with the package importable.

    cgrpy                        list the tools, one line each
    cgrpy <tool> [args]          rdy2use/<tool>.py
    cgrpy examples\\qchic.py     any script, by path - relative to here or to where you are

The launchers (cgrpy.cmd, cgrpy) only set up the environment and hand the command line over as it is;
everything else happens here, once, for both shells. That is also why the command line survives intact:
a regex with `|` in it, a type name with a space - nothing is re-parsed by a shell on the way.
"""

import ast
import runpy
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TOOLS = ROOT / "rdy2use"
EXAMPLES = ROOT / "examples"


def summary(script: Path) -> str:
    try:
        doc = ast.get_docstring(ast.parse(script.read_text(encoding="utf-8"))) or ""
    except (OSError, SyntaxError):
        return ""
    return doc.strip().splitlines()[0] if doc.strip() else ""


def listing():
    print("CgrPy - tools for reading Audiosurf from the outside. README.md has the whole story.\n")
    print("Tools (cgrpy <tool>, or with no arguments for its usage):\n")
    for script in sorted(TOOLS.glob("*.py")):
        print(f"  {script.stem:<11} {summary(script)}")
    print("\nExamples (cgrpy examples\\<name>.py) - patterns to copy, not maintained tools:\n")
    for script in sorted(EXAMPLES.glob("*.py")):
        print(f"  {script.name:<11} {summary(script)}")


def resolve(name: str):
    for candidate in (TOOLS / f"{name}.py", TOOLS / name, Path(name), ROOT / name):
        if candidate.is_file():
            return candidate.resolve()
    return None


def main():
    sys.path[:0] = [str(ROOT), str(TOOLS)]
    if len(sys.argv) < 2:
        listing()
        return 0
    script = resolve(sys.argv[1])
    if script is None:
        print(f"cgrpy: no tool or script called {sys.argv[1]!r}. Run `cgrpy` for the list.", file=sys.stderr)
        return 2
    sys.argv = [str(script)] + sys.argv[2:]
    sys.path.insert(0, str(script.parent))
    runpy.run_path(str(script), run_name="__main__")
    return 0


if __name__ == "__main__":
    sys.exit(main())
