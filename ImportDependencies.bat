@REM
@REM DemoSuite
@REM

REM fmt lib
COPY ..\..\Binaries\fmt\vs2017\Debug\*.lib demo-suite\vendor\lib\vs2017\Win32\Debug
COPY ..\..\Binaries\fmt\vs2017\Release\*.lib demo-suite\vendor\lib\vs2017\Win32\Release

COPY ..\..\Binaries\fmt\vs2022\Debug\*.lib demo-suite\vendor\lib\vs2022\x64\Debug
COPY ..\..\Binaries\fmt\vs2022\Release\*.lib demo-suite\vendor\lib\vs2022\x64\Release

REM FreeImage 2017
COPY ..\..\Binaries\FreeImage\vs2017\bin\x86\Debug\*.dll demo-suite\vendor\bin\vs2017\Win32\Debug
COPY ..\..\Binaries\FreeImage\vs2017\bin\x86\Release\*.dll demo-suite\vendor\bin\vs2017\Win32\Release

COPY ..\..\Binaries\FreeImage\vs2017\lib\x86\Debug\*.lib demo-suite\vendor\lib\vs2017\Win32\Debug
COPY ..\..\Binaries\FreeImage\vs2017\lib\x86\Release\*.lib demo-suite\vendor\lib\vs2017\Win32\Release

REM FreeImage 2022
COPY ..\..\Binaries\FreeImage\vs2022\bin\x64\Debug\*.dll demo-suite\vendor\bin\vs2022\x64\Debug
COPY ..\..\Binaries\FreeImage\vs2022\bin\x64\Release\*.dll demo-suite\vendor\bin\vs2022\x64\Release

COPY ..\..\Binaries\FreeImage\vs2022\lib\x64\Debug\*.lib demo-suite\vendor\lib\vs2022\x64\Debug
COPY ..\..\Binaries\FreeImage\vs2022\lib\x64\Release\*.lib demo-suite\vendor\lib\vs2022\x64\Release

REM SDL2 2017
COPY ..\..\Binaries\SDL2\vs2017\Debug\*.dll demo-suite\vendor\bin\vs2017\Win32\Debug
COPY ..\..\Binaries\SDL2\vs2017\Release\*.dll demo-suite\vendor\bin\vs2017\Win32\Release

COPY ..\..\Binaries\SDL2\vs2017\Debug\*.lib demo-suite\vendor\lib\vs2017\Win32\Debug
COPY ..\..\Binaries\SDL2\vs2017\Release\*.lib demo-suite\vendor\lib\vs2017\Win32\Release

REM SDL2 2022
COPY ..\..\Binaries\SDL2\vs2022\Debug\*.dll demo-suite\vendor\bin\vs2022\x64\Debug
COPY ..\..\Binaries\SDL2\vs2022\Release\*.dll demo-suite\vendor\bin\vs2022\x64\Release

COPY ..\..\Binaries\SDL2\vs2022\Debug\*.lib demo-suite\vendor\lib\vs2022\x64\Debug
COPY ..\..\Binaries\SDL2\vs2022\Release\*.lib demo-suite\vendor\lib\vs2022\x64\Release

REM Assimp 2017
COPY ..\..\Binaries\assimp\vs2017\bin\Debug\*.dll demo-suite\vendor\bin\vs2017\Win32\Debug
COPY ..\..\Binaries\assimp\vs2017\bin\Release\*.dll demo-suite\vendor\bin\vs2017\Win32\Release

COPY ..\..\Binaries\assimp\vs2017\lib\Debug\*.lib demo-suite\vendor\lib\vs2017\Win32\Debug
COPY ..\..\Binaries\assimp\vs2017\lib\Release\*.lib demo-suite\vendor\lib\vs2017\Win32\Release

REM Assimp 2022
COPY ..\..\Binaries\assimp\vs2022\bin\Debug\*.dll demo-suite\vendor\bin\vs2022\x64\Debug
COPY ..\..\Binaries\assimp\vs2022\bin\Release\*.dll demo-suite\vendor\bin\vs2022\x64\Release

COPY ..\..\Binaries\assimp\vs2022\lib\Debug\*.lib demo-suite\vendor\lib\vs2022\x64\Debug
COPY ..\..\Binaries\assimp\vs2022\lib\Release\*.lib demo-suite\vendor\lib\vs2022\x64\Release

REM GLEW 2017
COPY ..\..\Binaries\glew\vs2017\bin\x86\Debug\*.dll demo-suite\vendor\bin\vs2017\Win32\Debug
COPY ..\..\Binaries\glew\vs2017\bin\x86\Release\*.dll demo-suite\vendor\bin\vs2017\Win32\Release

COPY ..\..\Binaries\glew\vs2017\lib\x86\Debug\*.lib demo-suite\vendor\lib\vs2017\Win32\Debug
COPY ..\..\Binaries\glew\vs2017\lib\x86\Release\*.lib demo-suite\vendor\lib\vs2017\Win32\Release

REM GLEW 2022
COPY ..\..\Binaries\glew\vs2022\bin\x64\Debug\*.dll demo-suite\vendor\bin\vs2022\x64\Debug
COPY ..\..\Binaries\glew\vs2022\bin\x64\Release\*.dll demo-suite\vendor\bin\vs2022\x64\Release

COPY ..\..\Binaries\glew\vs2022\lib\x64\Debug\*.lib demo-suite\vendor\lib\vs2022\x64\Debug
COPY ..\..\Binaries\glew\vs2022\lib\x64\Release\*.lib demo-suite\vendor\lib\vs2022\x64\Release

@REM
@REM ModelConvert
@REM

REM fmt lib
COPY ..\..\Binaries\fmt\vs2017\Debug\*.lib model-convert\vendor\lib\vs2017\Win32\Debug
COPY ..\..\Binaries\fmt\vs2017\Release\*.lib model-convert\vendor\lib\vs2017\Win32\Release

COPY ..\..\Binaries\fmt\vs2022\Debug\*.lib demo-suite\vendor\lib\vs2022\x64\Debug
COPY ..\..\Binaries\fmt\vs2022\Release\*.lib demo-suite\vendor\lib\vs2022\x64\Release

REM Assimp 2017
COPY ..\..\Binaries\assimp\vs2017\bin\Debug\*.dll model-convert\vendor\bin\vs2017\Win32\Debug
COPY ..\..\Binaries\assimp\vs2017\bin\Release\*.dll model-convert\vendor\bin\vs2017\Win32\Release

COPY ..\..\Binaries\assimp\vs2017\lib\Debug\*.lib demo-suite\vendor\lib\vs2017\Win32\Debug
COPY ..\..\Binaries\assimp\vs2017\lib\Release\*.lib demo-suite\vendor\lib\vs2017\Win32\Release

REM Assimp 2022
COPY ..\..\Binaries\assimp\vs2022\bin\Debug\*.dll model-convert\vendor\bin\vs2022\x64\Debug
COPY ..\..\Binaries\assimp\vs2022\bin\Release\*.dll model-convert\vendor\bin\vs2022\x64\Release

COPY ..\..\Binaries\assimp\vs2022\lib\Debug\*.lib model-convert\vendor\lib\vs2022\x64\Debug
COPY ..\..\Binaries\assimp\vs2022\lib\Release\*.lib model-convert\vendor\lib\vs2022\x64\Release

