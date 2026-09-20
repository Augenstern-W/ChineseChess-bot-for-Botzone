@echo off
cd /d "%~dp0"
set "PATH=%~dp0tools\mingw64\bin;%PATH%"

if not exist bin\chess_server.exe (
    echo Not built yet. Run build.bat first.
    pause
    exit /b 1
)

echo Starting local server, browser will open http://localhost:8080
echo Close this window or press Ctrl+C to stop.
start "" http://localhost:8080
bin\chess_server.exe 8080
