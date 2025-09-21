@echo off
REM build.bat - Windows build script

echo Building Chat Application...

REM Check if g++ is available
where g++ >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: g++ compiler not found!
    echo Please install MinGW-w64 or MSYS2
    echo Download from: https://www.msys2.org/
    pause
    exit /b 1
)

REM Build server
echo Building server...
g++ -std=c++17 -Wall -Wextra -g -pthread server.cpp server_manager.cpp config_manager.cpp interserver_protocol.cpp -o server.exe -lws2_32
if %ERRORLEVEL% NEQ 0 (
    echo Error building server!
    pause
    exit /b 1
)

REM Build client
echo Building client...
g++ -std=c++17 -Wall -Wextra -g -pthread client.cpp -o client.exe -lws2_32
if %ERRORLEVEL% NEQ 0 (
    echo Error building client!
    pause
    exit /b 1
)

echo.
echo Build successful!
echo.
echo To run:
echo   Server: server.exe
echo   Client: client.exe
echo.
pause
