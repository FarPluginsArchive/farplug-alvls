@echo off
@cls

call "%VS100COMNTOOLS%vsvars32.bat"
nmake /f makefile_vc
rem nmake DEBUG=1 -f makefile_vc

