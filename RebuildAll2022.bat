REM utils
cd ext\utils
git pull origin master
call RebuildAll2022.bat

cd ..\..

REM MassivePolyPusher
devenv build/vs2022/MassivePolyPusher.sln /Rebuild "Release|Win32"
devenv build/vs2022/MassivePolyPusher.sln /Rebuild "Debug|Win32"


