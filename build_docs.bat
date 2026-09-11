@echo off
setlocal enabledelayedexpansion
::/ @file
::/ @brief Builds the FC PICO developer reference into docs/html.
::/
::/ Run from the repository root. Requires Doxygen 1.18+, Graphviz (`dot`) and
::/ Python 3 -- the last of these drives tools/doxygen/doxyfilter.py, which
::/ shadows the 6502, PIO, MML and batch sources into a form Doxygen can parse.
::/
::/ Exits non-zero if any prerequisite is missing or if Doxygen emitted
::/ warnings, so the script is safe to chain in a pre-commit hook.

cd /d "%~dp0"

set "DOXYGEN=doxygen"
where doxygen >nul 2>&1 || set "DOXYGEN=C:\Program Files\doxygen\bin\doxygen.exe"
if not exist "%DOXYGEN%" (
    where doxygen >nul 2>&1 || (
        echo [ERROR] Doxygen not found. Install it or add it to PATH.
        exit /b 1
    )
)

where python >nul 2>&1 || (
    echo [ERROR] python not found on PATH; the Doxygen input filter needs it.
    exit /b 1
)

where dot >nul 2>&1 || (
    echo [WARN] Graphviz 'dot' not found - diagrams will be missing.
)

if exist docs\doxygen-warnings.log del /q docs\doxygen-warnings.log

echo [1/2] Running Doxygen...
"%DOXYGEN%" Doxyfile
if errorlevel 1 (
    echo [ERROR] Doxygen exited with an error.
    exit /b 1
)

echo [2/2] Checking warnings...
set WARNCOUNT=0
if exist docs\doxygen-warnings.log (
    for /f %%A in ('find /c /v "" ^< docs\doxygen-warnings.log') do set WARNCOUNT=%%A
)

if "%WARNCOUNT%"=="0" (
    echo.
    echo Docs built cleanly: docs\html\index.html
    exit /b 0
)

echo.
echo [FAIL] %WARNCOUNT% warning line^(s^) in docs\doxygen-warnings.log:
echo ------------------------------------------------------------
type docs\doxygen-warnings.log
echo ------------------------------------------------------------
exit /b 1
