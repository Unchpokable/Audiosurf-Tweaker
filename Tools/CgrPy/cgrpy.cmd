@echo off
rem cgrpy - run a CgrPy tool with its dependencies handled. Pair: ./cgrpy (Git Bash); keep them in step.
rem
rem   cgrpy                          list the tools
rem   cgrpy chtypes array            rdy2use\chtypes.py with "array"
rem   cgrpy vtdump --type "Array Vector"
rem   cgrpy examples\qchic.py        anything else, by path
rem
rem From cmd in this folder it is .\cgrpy, not cgrpy: with NoDefaultCurrentDirectoryInExePath set (agents
rem set it), cmd does not look in the current folder. An editor opened from Audiosurf Tweaker has this
rem folder on its PATH, and there plain cgrpy works anywhere.
rem
rem All this does is set up the environment and hand the command line to launcher.py unchanged, so a
rem regex with | in it or a type name with a space arrive as typed. uv builds the virtualenv on first use.
rem Everything generated stays inside this folder (.venv, .cache) - uv's own cache and the Python it may
rem download included, unless you have pointed UV_CACHE_DIR / UV_PYTHON_INSTALL_DIR somewhere yourself.

setlocal
set "ROOT=%~dp0"
rem %~dp0 ends in a backslash, and "...\CgrPy\" makes the trailing backslash escape the closing quote.
set "PROJ=%ROOT:~0,-1%"
set "PYTHONIOENCODING=utf-8"
if not defined UV_CACHE_DIR set "UV_CACHE_DIR=%ROOT%.cache\uv"
if not defined UV_PYTHON_INSTALL_DIR set "UV_PYTHON_INSTALL_DIR=%ROOT%.cache\python"

where uv >nul 2>&1 || goto :no_uv

uv run --quiet --project "%PROJ%" python "%ROOT%launcher.py" %*
exit /b %ERRORLEVEL%

:no_uv
echo cgrpy needs uv, and README.md says so in its very first section.
echo.
echo     winget install --id=astral-sh.uv -e
echo.
echo Then open a new terminal, so that uv is on its PATH, and run this again. uv sets up Python and
echo everything else by itself; see https://docs.astral.sh/uv/ for other ways to install it.
exit /b 1
