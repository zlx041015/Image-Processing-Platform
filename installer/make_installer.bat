@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%i in ("%SCRIPT_DIR%..") do set ROOT_DIR=%%~fi

set QT_DIR=D:\QT\6.11.0\msvc2022_64
set BUILD_DIR=%ROOT_DIR%\build
set RELEASE_DIR=%ROOT_DIR%\release
set APP_EXE=BMPViewerQt.exe
set ISCC=E:\Inno Setup 6\ISCC.exe

echo SCRIPT_DIR=%SCRIPT_DIR%
echo ROOT_DIR=%ROOT_DIR%
echo BUILD_DIR=%BUILD_DIR%
echo RELEASE_DIR=%RELEASE_DIR%

if not exist "%BUILD_DIR%" (
    echo ERROR: build directory not found: "%BUILD_DIR%"
    goto :fail
)

if not exist "%BUILD_DIR%\Release\%APP_EXE%" (
    echo ERROR: executable not found: "%BUILD_DIR%\Release\%APP_EXE%"
    goto :fail
)

echo [1/5] Building project...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto :fail

echo [2/5] Preparing release folder...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"
copy /y "%BUILD_DIR%\Release\%APP_EXE%" "%RELEASE_DIR%\"
if errorlevel 1 goto :fail

echo [3/5] Deploying Qt runtime...
"%QT_DIR%\bin\windeployqt.exe" "%RELEASE_DIR%\%APP_EXE%"
if errorlevel 1 goto :fail

echo [4/5] Building installer...
"%ISCC%" "%SCRIPT_DIR%BuildInstaller.iss"
if errorlevel 1 goto :fail

echo [5/5] Done.
echo Installer created under installer\output
goto :eof

:fail
echo.
echo FAILED. Please check the error messages above.
exit /b 1