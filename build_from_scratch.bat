@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "WITH_LFS=false"
set "CONFIG=Release"
set "BUILD_DIR=build-windows"

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="/with-lfs" (
    set "WITH_LFS=true"
    shift
    goto parse_args
)
if /i "%~1"=="/config" (
    if "%~2"=="" (
        set "ERROR_MESSAGE=/config requires a value"
        goto fatal
    )
    set "CONFIG=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="/build-dir" (
    if "%~2"=="" (
        set "ERROR_MESSAGE=/build-dir requires a value"
        goto fatal
    )
    set "BUILD_DIR=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="/?" goto usage_success
if /i "%~1"=="/help" goto usage_success
set "ERROR_MESSAGE=unknown option: %~1 (run with /? for usage)"
goto fatal

:args_done
where git >nul 2>&1
if errorlevel 1 (
    set "ERROR_MESSAGE=Git is required but was not found on PATH"
    goto fatal
)
where cmake >nul 2>&1
if errorlevel 1 (
    set "ERROR_MESSAGE=CMake is required but was not found on PATH"
    goto fatal
)

for %%I in ("%~dp0.") do set "ROOT_DIR=%%~fI"
pushd "%ROOT_DIR%"
if errorlevel 1 (
    set "ERROR_MESSAGE=could not enter repository directory: %ROOT_DIR%"
    goto fatal
)
set "PUSHD_DONE=true"

git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    set "ERROR_MESSAGE=%ROOT_DIR% is not a Git checkout"
    goto fatal
)
if not exist "CMakeLists.txt" (
    set "ERROR_MESSAGE=run this script from the MassivePolyPusher checkout"
    goto fatal
)
if not exist ".gitmodules" (
    set "ERROR_MESSAGE=run this script from the MassivePolyPusher checkout"
    goto fatal
)

if /i "%WITH_LFS%"=="true" (
    git lfs version >nul 2>&1
    if errorlevel 1 goto missing_lfs
)

echo Synchronizing and checking out all submodules...
git submodule sync --recursive
if errorlevel 1 (
    set "ERROR_MESSAGE=failed to synchronize submodules"
    goto fatal
)
git submodule update --init --recursive
if errorlevel 1 (
    set "ERROR_MESSAGE=failed to check out submodules"
    goto fatal
)

set "SUBMODULE_STATUS_FILE=%TEMP%\massive-poly-pusher-submodules-%RANDOM%-%RANDOM%.txt"
git submodule status --recursive > "%SUBMODULE_STATUS_FILE%"
if errorlevel 1 (
    set "ERROR_MESSAGE=failed to inspect submodules"
    goto fatal
)
type "%SUBMODULE_STATUS_FILE%"
findstr /r /b /c:"[+U-]" "%SUBMODULE_STATUS_FILE%" >nul
if not errorlevel 1 (
    del /q "%SUBMODULE_STATUS_FILE%" >nul 2>&1
    set "ERROR_MESSAGE=one or more submodules are not checked out at the commits recorded by their parent"
    goto fatal
)
del /q "%SUBMODULE_STATUS_FILE%" >nul 2>&1

if /i "%WITH_LFS%"=="true" (
    echo Downloading MassivePolyPusher Git LFS files...
    git lfs install --local
    if errorlevel 1 (
        set "ERROR_MESSAGE=failed to initialize Git LFS"
        goto fatal
    )
    git lfs pull
    if errorlevel 1 (
        set "ERROR_MESSAGE=failed to download Git LFS files"
        goto fatal
    )
)

if not defined BUILD_DIR (
    set "ERROR_MESSAGE=refusing to remove an empty build directory"
    goto fatal
)
for %%I in ("%BUILD_DIR%") do set "BUILD_DIR=%%~fI"
if /i "%BUILD_DIR%"=="%ROOT_DIR%" (
    set "ERROR_MESSAGE=refusing to remove unsafe build directory: %BUILD_DIR%"
    goto fatal
)
for %%I in ("%BUILD_DIR%\..") do set "BUILD_PARENT=%%~fI"
if /i "%BUILD_DIR%"=="%BUILD_PARENT%" (
    set "ERROR_MESSAGE=refusing to remove unsafe build directory: %BUILD_DIR%"
    goto fatal
)

set "CHECK_DIR=%ROOT_DIR%"
:check_repository_parent
if /i "%CHECK_DIR%"=="%BUILD_DIR%" (
    set "ERROR_MESSAGE=refusing to remove a directory containing this checkout: %BUILD_DIR%"
    goto fatal
)
for %%I in ("%CHECK_DIR%\..") do set "CHECK_PARENT=%%~fI"
if /i "%CHECK_DIR%"=="%CHECK_PARENT%" goto repository_parent_checked
set "CHECK_DIR=%CHECK_PARENT%"
goto check_repository_parent

:repository_parent_checked
rem CMake deliberately places final artifacts under source\build even when its
rem binary tree is elsewhere, so both locations are build output.
set "OUTPUT_DIR=%ROOT_DIR%\build"
echo Removing previous build output...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "%BUILD_DIR%" (
    set "ERROR_MESSAGE=could not remove build directory: %BUILD_DIR%"
    goto fatal
)
if /i not "%OUTPUT_DIR%"=="%BUILD_DIR%" if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
if exist "%OUTPUT_DIR%" (
    set "ERROR_MESSAGE=could not remove output directory: %OUTPUT_DIR%"
    goto fatal
)

echo Configuring %CONFIG% build in %BUILD_DIR%...
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE="%CONFIG%"
if errorlevel 1 (
    set "ERROR_MESSAGE=CMake configuration failed"
    goto fatal
)

echo Building MassivePolyPusher and dependencies...
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --parallel
if errorlevel 1 (
    set "ERROR_MESSAGE=build failed"
    goto fatal
)

echo Build completed successfully.
echo Binaries: %ROOT_DIR%\build\bin\%CONFIG%
popd
exit /b 0

:usage_success
call :usage
exit /b 0

:missing_lfs
set "ERROR_MESSAGE=/with-lfs requires Git LFS, but 'git lfs' is not installed. Install it with: winget install GitHub.GitLFS"
goto fatal

:usage
echo Usage: build_from_scratch.bat [options]
echo.
echo Configure and build MassivePolyPusher and its dependencies from scratch.
echo.
echo Options:
echo   /with-lfs          Download this repository's Git LFS files.
echo   /config CONFIG     CMake build configuration ^(default: Release^).
echo   /build-dir DIR     Build directory, relative to the repository root unless
echo                      absolute ^(default: build-windows^).
echo(  /?, /help          Show this help.
echo.
echo Environment:
echo   CC, CXX               Select the C and C++ compilers during configuration.
echo   CMAKE_GENERATOR       Select a CMake generator.
echo   CMAKE_BUILD_PARALLEL_LEVEL
echo                         Limit the number of parallel build jobs.
exit /b 0

:fatal
if defined PUSHD_DONE popd
>&2 <nul set /p "=error: %ERROR_MESSAGE%"
>&2 echo.
exit /b 1
