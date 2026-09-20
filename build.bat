@echo off
setlocal
cd /d "%~dp0"
set "PATH=%~dp0tools\mingw64\bin;%PATH%"

if not exist bin mkdir bin

echo [1/6] Generating embedded web resources (web_resources.h) ...
powershell -ExecutionPolicy Bypass -File "%~dp0tools\gen_web_res.ps1"
if errorlevel 1 goto err

echo [2/6] Compiling engine self-test (selftest.exe) ...
g++ -O2 -std=c++14 -Iengine test\selftest.cpp engine\chess.cpp engine\search.cpp -o bin\selftest.exe
if errorlevel 1 goto err

echo [3/6] Compiling windowed GUI player (chess_gui.exe, recommended) ...
g++ -O2 -std=c++14 -mwindows -static -static-libgcc -static-libstdc++ -Iengine gui\gui_main.cpp engine\chess.cpp engine\search.cpp -o bin\chess_gui.exe
if errorlevel 1 goto err

echo [4/6] Compiling console player (chess_console.exe, no browser needed) ...
g++ -O2 -std=c++14 -static -static-libgcc -static-libstdc++ -Iengine console\console_main.cpp engine\chess.cpp engine\search.cpp -o bin\chess_console.exe
if errorlevel 1 goto err

echo [5/6] Compiling standalone web server (chess_server.exe, single exe) ...
g++ -O2 -std=c++14 -static -static-libgcc -static-libstdc++ -Iengine server\server_main.cpp engine\chess.cpp engine\search.cpp -o bin\chess_server.exe -lws2_32 -lshell32
if errorlevel 1 goto err

echo [6/6] Generating Botzone single-file submission (botzone_submit.cpp) ...
powershell -ExecutionPolicy Bypass -File "%~dp0tools\make_botzone.ps1"
if errorlevel 1 goto err

echo.
echo Build OK:
echo   bin\chess_gui.exe           windowed GUI player (recommended, double-click to play)
echo   bin\chess_console.exe       console player (no browser, double-click to play)
echo   bin\chess_server.exe        standalone web server (double-click to play)
echo   bin\selftest.exe            engine self-test (AI vs AI)
echo   botzone\botzone_submit.cpp  single file for Botzone
echo.
echo GUI play: run bin\chess_gui.exe directly.
echo Console play: run bin\chess_console.exe directly.
echo Web play: run bin\chess_server.exe - it starts the server and opens your browser.
goto :eof

:err
echo.
echo Build FAILED, check the errors above.
exit /b 1
