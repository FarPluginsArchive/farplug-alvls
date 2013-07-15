@echo off

echo Не забудьте настроить пути!
echo -=-=-=-=-=-=-=-=-=-=-=-=-=

  set far=D:\Far3
  set updtmp=D:\Far3\UpdateEx
  set cache=%updtmp%\UpdateEx.dll.x86.cache
  set _7z=%ProgramFiles%\7-Zip\7z.exe

  set our=N
  set "undo="
  for /f "tokens=1,2 delims=|=" %%a in (%cache%) do if "" == "%undo%" call :proc_line %%a %%b
  if not "" == "%undo%" (
    echo Undo: "%_7z%" x "%updtmp%\%undo%" -o"%far%" -y
    echo Для продолжения нажмите любую клавишу...
    pause>nul
    "%_7z%" x "%updtmp%\%undo%" -o"%far%" -y
  )
goto :EOF
 
:proc_line
  if "[00000000-0000-0000-0000-000000000000]" == "%1" set "our=Y"& goto :EOF
  set t=%1
  if "[" == "%t:~0,1%" set "our=N"& goto :EOF
  if /i "Undo" == "%1" if "Y" == "%our%" set "undo=%2"
goto :EOF