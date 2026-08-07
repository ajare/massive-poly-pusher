@echo off
setlocal
set MSBUILD=msbuild

%MSBUILD% ext\utils\build\vs2026\Utils.sln /t:Build /p:Platform=x64 /p:Configuration=Debug /m || exit /b 1
%MSBUILD% ext\utils\build\vs2026\Utils.sln /t:Build /p:Platform=x64 /p:Configuration=Release /m || exit /b 1
%MSBUILD% build\vs2026\MassivePolyPusher.sln /t:Build /p:Platform=x64 /p:Configuration=Debug /m || exit /b 1
%MSBUILD% build\vs2026\MassivePolyPusher.sln /t:Build /p:Platform=x64 /p:Configuration=Release /m || exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\ValidatePipelineEditorPhase10.ps1 -Configuration Debug || exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\ValidatePipelineEditorPhase10.ps1 -Configuration Release || exit /b 1

endlocal
