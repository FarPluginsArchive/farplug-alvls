@echo off
@cls

 @call "%VS100COMNTOOLS%..\..\VC\vcvarsall.bat" amd64
nmake AMD64=1 -f makefile_vc
