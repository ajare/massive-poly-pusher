@echo off
setlocal
set TOOLSET=%1
set PLATFORM=%2
set CONFIG=%3
set OUT=%~dp0build\%TOOLSET%\bin\%PLATFORM%\%CONFIG%
if not exist "%OUT%" mkdir "%OUT%"
copy /Y "%~dp0..\vendor\bin\%TOOLSET%\%PLATFORM%\%CONFIG%\*.dll" "%OUT%" >nul
copy /Y "%~dp0..\ext\utils\build\%TOOLSET%\bin\%PLATFORM%\%CONFIG%\*.dll" "%OUT%" >nul
for %%P in (mpp mpp-mesh mpp-program mpp-resource-parsers) do copy /Y "%~dp0..\%%P\build\%TOOLSET%\bin\%PLATFORM%\%CONFIG%\*.dll" "%OUT%" >nul
if not exist "%OUT%\resources" mkdir "%OUT%\resources"
xcopy /E /I /Y "%~dp0resources" "%OUT%\resources" >nul 2>nul
endlocal
