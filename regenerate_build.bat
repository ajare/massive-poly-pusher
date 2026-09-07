@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "CONFIG=Release"
set "BUILD_DIR=build"

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--config" (
    if "%~2"=="" (
        set "ERROR_MESSAGE=--config requires a value"
        goto fatal
    )
    set "CONFIG=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--build-dir" (
    if "%~2"=="" (
        set "ERROR_MESSAGE=--build-dir requires a value"
        goto fatal
    )
    set "BUILD_DIR=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-h" goto usage_success
if /i "%~1"=="--help" goto usage_success
set "ERROR_MESSAGE=unknown option: %~1 (run with --help for usage)"
goto fatal

:args_done
for %%I in ("%~dp0.") do set "ROOT_DIR=%%~fI"
pushd "%ROOT_DIR%"
if errorlevel 1 exit /b %ERRORLEVEL%
for %%I in ("%BUILD_DIR%") do set "BUILD_DIR=%%~fI"
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE="%CONFIG%"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%

:usage_success
call :usage
exit /b 0

:usage
echo Usage: regenerate_build.bat [--config CONFIG] [--build-dir DIR]
echo.
echo Regenerate the MassivePolyPusher build system with CMake.
echo.
echo Options:
echo   --config CONFIG    CMake build configuration ^(default: Release^).
echo   --build-dir DIR    Build directory, relative to the repository root unless
echo                      absolute ^(default: build^).
echo   -h, --help         Show this help.
exit /b 0

:fatal
>&2 echo error: %ERROR_MESSAGE%
exit /b 1
