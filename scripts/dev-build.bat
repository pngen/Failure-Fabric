@echo off
setlocal
set CONFIG=%1
if "%~1"=="" set CONFIG=Release
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cmake -S "%~dp0.." -B "%~dp0..\build" -G "Ninja Multi-Config"
if errorlevel 1 exit /b %errorlevel%
cmake --build "%~dp0..\build" --config %CONFIG%
