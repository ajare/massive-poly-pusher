REM utils
cd ext\utils
git pull origin master
call RebuildAll2022.bat

cd ..\..

REM MassivePolyPusher
devenv build/vs2022/MassivePolyPusher.sln /Rebuild "Release|x64"
devenv build/vs2022/MassivePolyPusher.sln /Rebuild "Debug|x64"


