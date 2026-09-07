@echo off
rem Run any tool in rdy2use/ (or examples/, or a path you give) with the dependencies handled.
rem
rem   uvrun                          list the tools
rem   uvrun chtypes array            rdy2use\chtypes.py with "array"
rem   uvrun vtdump --type "Array Vector"
rem   uvrun examples\qchic.py        anything else, by path
rem
rem uv creates and syncs the virtualenv on first use, so there is no separate setup step. The name
rem may be given with or without the .py.
rem
rem PYTHONPATH covers both the package root and rdy2use/, because a couple of the tools import a
rem helper from a sibling (disasm and symbols reuse vtdump's module lookup) and running a script by
rem path does not put its own directory on the path the way `-m` would.

setlocal
set "ROOT=%~dp0"
rem %~dp0 ends in a backslash, and "...\CgrPy\" makes the trailing backslash escape the closing
rem quote - uv then sees the rest of the command line as part of the path. PROJ is the same directory
rem without it, and is what gets quoted.
set "PROJ=%ROOT:~0,-1%"
set "PYTHONPATH=%ROOT%;%ROOT%rdy2use;%PYTHONPATH%"

where uv >nul 2>&1 || (
    echo uvrun: uv is not on PATH. Install it from https://docs.astral.sh/uv/ - it handles the
    echo        virtualenv and dependencies itself, so nothing else needs setting up.
    exit /b 1
)

if "%~1"=="" goto :list

set "SCRIPT=%~1"
if exist "%ROOT%rdy2use\%SCRIPT%.py" set "SCRIPT=%ROOT%rdy2use\%SCRIPT%.py"
if exist "%ROOT%rdy2use\%SCRIPT%"    set "SCRIPT=%ROOT%rdy2use\%SCRIPT%"

shift
rem %* still holds the original first argument after shift, so the remaining arguments are collected
rem by hand rather than with %*.
set "ARGS="
:collect
if "%~1"=="" goto :go
set "ARGS=%ARGS% "%~1""
shift
goto :collect

:go
uv run --project "%PROJ%" python "%SCRIPT%"%ARGS%
exit /b %ERRORLEVEL%

:list
echo Tools in rdy2use:
echo.
for %%F in ("%ROOT%rdy2use\*.py") do echo    %%~nF
echo.
echo   uvrun ^<tool^> [args]        see README.md, or run a tool with no arguments for its usage
exit /b 0
