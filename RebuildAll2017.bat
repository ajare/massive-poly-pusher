REM utils
cd ext\utils
git pull origin master
call RebuildAll2017.bat

cd ..\..

REM MassivePolyPusher
devenv build/vs2017/MassivePolyPusher.sln /Rebuild "Release|Win32"
devenv build/vs2017/MassivePolyPusher.sln /Rebuild "Debug|Win32"


