@echo off

set Solution=
set Solution=%1

set Platform=
set Platform=%2

set Configuration=
set Configuration=%3

@rem Bail if we haven't got the parameters
if "%Solution%" == "" goto exit_noparam
if "%Platform%" == "" goto exit_noparam
if "%Configuration%" == "" goto exit_noparam

set Root=%~dp0

@rem
@rem BIN
@rem

@rem Create directory
set TargetBinDir="%Root%.\build\%Solution%\bin\%Platform%\%Configuration%"

if not exist %TargetBinDir% mkdir %TargetBinDir%

@rem Copy vendor
copy /Y "%Root%\..\vendor\bin\%Solution%\%Platform%\%Configuration%\*.*" %TargetBinDir%

@rem Copy utils
copy /Y "%Root%\..\ext\utils\build\%Solution%\bin\%Platform%\%Configuration%\*.dll" %TargetBinDir%

@rem Copy mpp
copy /Y "%Root%\..\mpp\build\%Solution%\bin\%Platform%\%Configuration%\*.dll" %TargetBinDir%

@rem Copy mpp-mesh
copy /Y "%Root%\..\mpp-mesh\build\%Solution%\bin\%Platform%\%Configuration%\*.dll" %TargetBinDir%

@rem Copy mpp-program
copy /Y "%Root%\..\mpp-program\build\%Solution%\bin\%Platform%\%Configuration%\*.dll" %TargetBinDir%

@rem Copy mpp-resource-parsers
copy /Y "%Root%\..\mpp-resource-parsers\build\%Solution%\bin\%Platform%\%Configuration%\*.dll" %TargetBinDir%

@rem Copy mpp-helper
copy /Y "%Root%\..\mpp-helper\build\%Solution%\bin\%Platform%\%Configuration%\*.dll" %TargetBinDir%

@rem Copy resources
copy /Y "%Root%\resources\bin\Common\*.*" %TargetBinDir%
copy /Y "%Root%\resources\bin\%Configuration%\*.*" %TargetBinDir%

goto exit_success

@rem ERRORS

:exit_noparam
@echo syntax: CopyBin.bat <solution> <platform> <configuration>
exit /b 1

@rem SUCCESS

:exit_success
exit /b 0
