@echo off
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b %errorlevel%
if not exist build mkdir build
rc /nologo /fo build\resource.res resource.rc
if errorlevel 1 exit /b %errorlevel%
cl /std:c++20 /EHsc /nologo /DNDEBUG /Fo:build\ main.cpp build\resource.res /Fe:build\TerminalRenderer.exe /link user32.lib gdi32.lib