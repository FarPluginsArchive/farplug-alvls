@echo off
@cls

 rem поместить файл в каталог "fardev"

cd /d "%~dp0"

rem  ========= set install dir =======================

 rem целевой каталог, который собственно и обновляем
set MyFarDir="G:\Far3"
 rem FExcept=1 для сборки FExcept
set FExcept=0
 rem можно задать иной подкаталог сорцев фара, патченых например
set UnicodeFar=unicode_far_w

rem  ========= SVN update ============================

if [%1]==[s] goto SKIPSVN
if [%1]==[/s] goto SKIPSVN
if [%2]==[s] goto SKIPSVN
if [%2]==[/s] goto SKIPSVN

title SVN update......
echo Update "fardev" .....
svn cleanup
svn update
if not "%UnicodeFar%"=="unicode_far" (
  cd "%~dp0%UnicodeFar%"
  echo Update "%UnicodeFar%".....
  svn update
)

:SKIPSVN

rem  ========= compiling =============================

title Far 3.0 x86 Release compiling......
call "%VS100COMNTOOLS%vsvars32.bat"
cd "%~dp0%UnicodeFar%"
nmake /f makefile_vc
if not exist "%~dp0%UnicodeFar%\Release.32.vc\far.exe" goto ERROR

rem  ========= update ================================

echo 
echo Far 3.0 x86 Release compiling.....  OK!
echo Update Far Manager....... kill Far.exe!
pause
taskkill /f /im far.exe
if exist "%~dp0%UnicodeFar%\Release.32.vc\Far.exe" xcopy "%~dp0%UnicodeFar%\Release.32.vc\Far.exe" "%MyFarDir%" /i /y
if exist "%~dp0%UnicodeFar%\Release.32.vc\Far.map" xcopy "%~dp0%UnicodeFar%\Release.32.vc\Far.map" "%MyFarDir%" /i /y
if exist "%~dp0%UnicodeFar%\Release.32.vc\FarEng.*" xcopy "%~dp0%UnicodeFar%\Release.32.vc\FarEng.*" "%MyFarDir%" /i /y
if exist "%~dp0%UnicodeFar%\Release.32.vc\FarRus.*" xcopy "%~dp0%UnicodeFar%\Release.32.vc\FarRus.*" "%MyFarDir%" /i /y
if exist "%~dp0%UnicodeFar%\changelog" xcopy "%~dp0%UnicodeFar%\changelog" "%MyFarDir%" /i /y
rmdir /s /q "%~dp0%UnicodeFar%\Release.32.vc" >nul
rmdir /s /q "%~dp0%UnicodeFar%\bootstrap" >nul

rem  ========= compiling =============================

if [%1]==[p] goto SKIPPLUG
if [%1]==[/p] goto SKIPPLUG
if [%2]==[p] goto SKIPPLUG
if [%2]==[/p] goto SKIPPLUG

title Plugins x86 Release compiling......
ren "%~dp0plugins\common\unicode\DlgBuilder.hpp" DlgBuilder._hpp
ren "%~dp0plugins\common\unicode\farcolor.hpp" farcolor._hpp
ren "%~dp0plugins\common\unicode\plugin.hpp" plugin._hpp
xcopy "%~dp0%UnicodeFar%\Include\*.hpp" "%~dp0plugins\common\unicode"
set WIDE=1

if "%FExcept%"=="1" (
  cd "%~dp0misc\fexcept"
  nmake /f makefile_vc
  if exist "%~dp0misc\fexcept\final.32W.vc\demangle32.dll" xcopy "%~dp0misc\fexcept\final.32W.vc\demangle32.dll" "%MyFarDir%\FExcept" /i /y
  if exist "%~dp0misc\fexcept\final.32W.vc\ExcDump.dll" xcopy "%~dp0misc\fexcept\final.32W.vc\ExcDump.dll" "%MyFarDir%\FExcept" /i /y
  if exist "%~dp0misc\fexcept\final.32W.vc\FExcept.dll" xcopy "%~dp0misc\fexcept\final.32W.vc\FExcept.dll" "%MyFarDir%\FExcept" /i /y
  if exist "%~dp0misc\fexcept\final.32W.vc\SetFarExceptionHandler.farconfig" xcopy "%~dp0misc\fexcept\final.32W.vc\SetFarExceptionHandler.farconfig" "%MyFarDir%\FExcept" /i /y
  cd "%~dp0plugins"
  rmdir /s /q "%~dp0misc\fexcept\final.32W.vc" >nul
  rmdir /s /q "%~dp0misc\fexcept\execdump\final.32.vc" >nul
)

set INSTALL=InsDir
cd "%~dp0plugins"
nmake /f makefile_all_vc

rem  ========= update ================================

move /y "%~dp0plugins\common\unicode\DlgBuilder._hpp" "%~dp0plugins\common\unicode\DlgBuilder.hpp"
move /y "%~dp0plugins\common\unicode\farcolor._hpp" "%~dp0plugins\common\unicode\farcolor.hpp"
move /y "%~dp0plugins\common\unicode\plugin._hpp" "%~dp0plugins\common\unicode\plugin.hpp"
rem rmdir /s /q "%~dp0%UnicodeFar%\Include" >nul
rmdir /s /q "%~dp0plugins\align\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\arclite\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\autowrap\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\brackets\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\compare\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\drawline\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\editcase\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\emenu\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\farcmds\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\filecase\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\ftp\final.32.vc" >nul
rmdir /s /q "%~dp0plugins\hlfviewer\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\macroview\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\multiarc\final.32.vc" >nul
rmdir /s /q "%~dp0plugins\network\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\proclist\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\tmppanel\final.32W.vc" >nul
rmdir /s /q "%~dp0plugins\common\CRT\obj.32.vc" >nul
del /s /q "%~dp0plugins\common\*.lib" >nul 2>nul

if exist "%~dp0plugins\%INSTALL%\final.32W" (
  cd "%~dp0plugins\%INSTALL%\final.32W"
  del /s /q *.lib >nul 2>nul
  del /s /q *.exp >nul 2>nul
  del /s /q *.pdb >nul 2>nul
  if exist "%MyFarDir%\Plugins" (
    xcopy "%~dp0plugins\%INSTALL%\final.32W\*" "%MyFarDir%\Plugins" /i /y /s /u
  ) else (
    xcopy "%~dp0plugins\%INSTALL%\final.32W\*" "%MyFarDir%\Plugins" /i /y /s
  )
  cd "%~dp0"
  rmdir /s /q "%~dp0plugins\%INSTALL%" >nul
)

 rem сборка моих плагинов!
cd ..
cd "farplug-alvls"
call CompileMyPlugVCx86.bat

:SKIPPLUG

rem  ========= exit ==================================

echo 
echo OK!
pause>nul
start "" /max "%MyFarDir%\far.exe" /i
exit

rem  ========= error =================================

:ERROR
if exist "%~dp0%UnicodeFar%\Release.32.vc" rmdir /s /q "%~dp0%UnicodeFar%\Release.32.vc" >nul
echo 
echo 
color 4f
echo error!
pause>nul
color 07
exit