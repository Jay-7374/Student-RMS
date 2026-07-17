@echo off
rem ================================================================
rem  Student Database System — Windows Build Script
rem  Usage: build.bat
rem
rem  Detects the C compiler automatically:
rem    gcc  (MinGW / Git-Bash)
rem    cl   (Microsoft Visual C++ / Build Tools)
rem ================================================================

setlocal EnableDelayedExpansion

echo.
echo ========================================
echo  Student Database System — Build Script
echo ========================================
echo.

rem ----------------------------------------------------------------
rem  Try gcc (MinGW) first
rem ----------------------------------------------------------------
where gcc >nul 2>&1
if not errorlevel 1 (
    echo [+] Compiler : gcc detected
    echo [+] Building server.exe ...
    gcc -Wall -Wextra -std=c99 ^
        -I. -Icommon -Iserver -Iclient ^
        server/server.c ^
        server/request_handler.c ^
        server/database.c ^
        src/student.c ^
        src/file_manager.c ^
        src/validation.c ^
        src/search.c ^
        src/sort.c ^
        src/protocol.c ^
        -o server.exe -lws2_32
    if errorlevel 1 ( echo [!] server.exe build FAILED & exit /b 1 )
    echo [+] server.exe built successfully.

    echo [+] Building client.exe ...
    gcc -Wall -Wextra -std=c99 ^
        -I. -Icommon -Iserver -Iclient ^
        client/client.c ^
        client/client_menu.c ^
        client/config.c ^
        src/student.c ^
        src/file_manager.c ^
        src/validation.c ^
        src/search.c ^
        src/sort.c ^
        -o client.exe -lws2_32
    if errorlevel 1 ( echo [!] client.exe build FAILED & exit /b 1 )
    echo [+] client.exe built successfully.
    goto :done
)

rem ----------------------------------------------------------------
rem  Try MSVC cl.exe
rem ----------------------------------------------------------------
where cl >nul 2>&1
if not errorlevel 1 (
    echo [+] Compiler : cl.exe (MSVC) detected
    echo [+] Building server.exe ...
    cl /nologo /W3 /MD /Fe:server.exe ^
        server\server.c server\request_handler.c server\database.c ^
        src\student.c src\file_manager.c src\validation.c ^
        src\search.c src\sort.c ^
        /I. /Icommon /Iserver /Iclient ^
        /link ws2_32.lib
    if errorlevel 1 ( echo [!] server.exe build FAILED & exit /b 1 )

    echo [+] Building client.exe ...
    cl /nologo /W3 /MD /Fe:client.exe ^
        client\client.c client\client_menu.c client\config.c ^
        src\student.c src\file_manager.c src\validation.c ^
        src\search.c src\sort.c ^
        /I. /Icommon /Iserver /Iclient ^
        /link ws2_32.lib
    if errorlevel 1 ( echo [!] client.exe build FAILED & exit /b 1 )
    goto :done
)

rem ----------------------------------------------------------------
rem  No compiler found
rem ----------------------------------------------------------------
echo [!] ERROR: No C compiler found on PATH.
echo.
echo     Install one of the following:
echo       - MinGW-w64  (provides gcc)
echo         https://www.mingw-w64.org/downloads/
echo       - Visual Studio Build Tools  (provides cl.exe)
echo         https://aka.ms/vs/17/release/vs_buildtools.exe
echo.
exit /b 1

:done
echo.
echo ========================================
echo  BUILD COMPLETE
echo  server.exe  --  run in Terminal 1
echo  client.exe  --  run in Terminal 2
echo ========================================
echo.
