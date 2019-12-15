@echo off

set Build=
set Build=%1

@rem Bail if we haven't got the 'build' parameter
if "%Build%" == "" goto exit_noparam

set Root=%~dp0

@rem
@rem BIN
@rem

@rem Create directory
set TargetBinDir="%Root%.\harness\3rd party\bin\%Build%"

if not exist %TargetBinDir% mkdir %TargetDir%

@rem Copy
copy /Y "%Root%\3rd party\bin\%Build%\*.dll" %TargetBinDir%
copy /Y "%Root%\bin\%Build%\*.dll" %TargetBinDir%

goto exit_success

@rem ERRORS

:exit_noparam
@echo syntax: CopyBin.bat build
exit 1

@rem SUCCESS

:exit_success
exit 0
