@echo off
@cls
rem =============== Use Microsoft Visual Studio .NET 2003 ======================

@call "C:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools\vsvars32.bat"

@set ApplicName=MacroDecrypt

@cl /Zp2 /O1igy /GF /Gr /GR- /GX- %ApplicName%.cpp /link /subsystem:console /machine:I386 /opt:nowin98 /noentry /nodefaultlib kernel32.lib msvcrt60.lib /map:"%ApplicName%.map" /merge:.rdata=.text
@if exist %ApplicName%.exp del %ApplicName%.exp>nul
@if exist %ApplicName%.obj del %ApplicName%.obj>nul
@if exist %ApplicName%.lib del %ApplicName%.lib>nul
@if exist %ApplicName%.res del %ApplicName%.res>nul
