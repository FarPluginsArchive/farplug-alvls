@echo off
@cls
rem =============== Use Microsoft Visual Studio .NET 2003 ======================

@call "C:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools\vsvars32.bat"

rem  ======================== Set name and version ... =========================

@set PlugName=SelJmp2
@set MyDir=%CD%
@set MyFarDir=C:\Program Files\Far

rem  ==================== Compile %PlugName%.dll file...========================

@cd ".."
@if exist %PlugName%.dll "%MyFarDir%\The Underscore\loader\LOADER.EXE" /u %PlugName%.dll
@if exist %PlugName%.dll del %PlugName%.dll>nul

@cd %MyDir%
@rc /l 0x4E4 %PlugName%.rc
@cl /Zp2 /O1igy /GF /Gr /GR- /GX- /LD %PlugName%.cpp /link /subsystem:console /machine:I386 /opt:nowin98 /noentry /nodefaultlib /def:%PlugName%.def kernel32.lib advapi32.lib user32.lib msvcrt60.lib shell32.lib %PlugName%.res /map:"..\%PlugName%.map" /out:"..\%PlugName%.dll" /merge:.rdata=.text
@if exist *.exp del *.exp>nul
@if exist *.obj del *.obj>nul
@if exist *.lib del *.lib>nul
@if exist *.res del *.res>nul

rem  ================= Load work %PlugName%.dll file... ========================

@cd %MyDir%
@cd ".."
@if exist %PlugName%.dll  "%MyFarDir%\The Underscore\loader\LOADER.EXE" /l %PlugName%.dll

echo ***************