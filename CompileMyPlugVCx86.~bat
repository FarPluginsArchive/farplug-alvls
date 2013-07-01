@echo off

cd "%~dp0"
 rem целевой каталог, который собственно и обновляем
set _MyFarDir="D:\Far3"
 rem можно задать иной подкаталог сорцев фара, патченых например
set _UnicodeFar=unicode_far_w

set new=0
if "%new%"=="1" (
  cd ..
  cd "fardev\%_UnicodeFar%\Include"
  xcopy "plugin.hpp" "%~dp0%advcmp3\SRC" /i /y
  xcopy "farcolor.hpp" "%~dp0%advcmp3\SRC" /i /y
  xcopy "plugin.hpp" "%~dp0%picviewadv3\SRC" /i /y
  xcopy "farcolor.hpp" "%~dp0%picviewadv3\SRC" /i /y
  xcopy "plugin.hpp" "%~dp0%visren3\SRC" /i /y
  xcopy "farcolor.hpp" "%~dp0%visren3\SRC" /i /y
)

cd "%~dp0%advcmp3\SRC"
call "%~dp0%advcmp3\SRC\CompilePlugVC.bat"

cd "%~dp0%picviewadv3\SRC"
call "%~dp0%picviewadv3\SRC\CompilePlugVC.bat"

cd "%~dp0%visren3\SRC"
call "%~dp0%visren3\SRC\CompilePlugVC.bat"

xcopy "%~dp0advcmp3\*" "%_MyFarDir%\Plugins\AdvCmp" /i /y
xcopy "%~dp0picviewadv3\*" "%_MyFarDir%\Plugins\PicViewAdv" /i /y
xcopy "%~dp0visren3\*" "%_MyFarDir%\Plugins\VisRen" /i /y

echo OK!
