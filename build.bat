@echo off
setlocal

echo === Q4RTX Build Script ===
echo.

:: Set your Quake 4 install path here
set Q4DIR=D:\SteamLibrary\steamapps\common\Quake 4

if "%1"=="clean" goto :clean
if "%1"=="deploy" goto :deploy
if "%1"=="help" goto :usage
if "%1"=="--help" goto :usage
if "%1"=="-h" goto :usage

:: Find Visual Studio
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul`) do set VSDIR=%%i
if not defined VSDIR (
    echo ERROR: Visual Studio not found!
    exit /b 1
)
echo Found VS at: %VSDIR%

:: Build 32-bit proxy DLL
echo.
echo [1/3] Configuring + building 32-bit proxy opengl32.dll...
cmake -S . -B build32 -A Win32
if errorlevel 1 (
    echo ERROR: CMake configure failed for proxy
    exit /b 1
)
cmake --build build32 --target q4rtx_proxy --config Release
if errorlevel 1 (
    echo ERROR: Failed to build proxy DLL
    exit /b 1
)
echo OK: build32\bin\proxy\Release\opengl32.dll

:: Build 64-bit renderer
echo.
echo [2/3] Configuring + building 64-bit DXR renderer...
cmake -S . -B build64 -A x64
if errorlevel 1 (
    echo ERROR: CMake configure failed for renderer
    exit /b 1
)
cmake --build build64 --target q4rtx_renderer --config Release
if errorlevel 1 (
    echo ERROR: Failed to build renderer
    exit /b 1
)
echo OK: build64\bin\renderer\Release\q4rtx_renderer.exe

:: Deploy
:deploy
echo.
echo [3/3] Deploying to: %Q4DIR%

if not exist "%Q4DIR%" (
    echo ERROR: Quake 4 directory not found: %Q4DIR%
    echo        Edit Q4DIR in build.bat to match your install path.
    exit /b 1
)

copy /Y "build32\bin\proxy\Release\opengl32.dll" "%Q4DIR%\opengl32.dll"
copy /Y "build64\bin\renderer\Release\q4rtx_renderer.exe" "%Q4DIR%\q4rtx_renderer.exe"
if not exist "%Q4DIR%\shaders" mkdir "%Q4DIR%\shaders"
copy /Y "shaders\*" "%Q4DIR%\shaders\"

echo.
echo === BUILD + DEPLOY COMPLETE ===
echo.
echo To use: just launch Quake 4 from Steam.
echo The proxy DLL auto-launches the renderer.
echo.
echo To remove: delete "%Q4DIR%\opengl32.dll" and "%Q4DIR%\q4rtx_renderer.exe"
goto :eof

:clean
echo Cleaning build directories...
if exist build32 rmdir /s /q build32
if exist build64 rmdir /s /q build64
echo Done.
goto :eof

:usage
echo Usage: build.bat [command]
echo.
echo Commands:
echo   (none)    Build proxy + renderer and deploy to Quake 4 folder
echo   deploy    Deploy already-built binaries (skip build)
echo   clean     Delete build32/ and build64/ directories
echo   help      Show this message
goto :eof
