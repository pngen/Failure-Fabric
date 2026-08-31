@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cmake -S "%~dp0.." -B "%~dp0..\build" -G "Ninja Multi-Config" -DCMAKE_BUILD_TYPE=Release %*
if errorlevel 1 exit /b %errorlevel%
cmake --build "%~dp0..\build" --config Release
