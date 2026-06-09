@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%i in ("%SCRIPT_DIR%..") do set ROOT_DIR=%%~fi

set BUILD_DIR=%ROOT_DIR%\build
set APP_EXE=MedicalImageAnalysisPlatform.exe
set ISCC=E:\Inno Setup 6\ISCC.exe

echo SCRIPT_DIR=%SCRIPT_DIR%
echo ROOT_DIR=%ROOT_DIR%
echo BUILD_DIR=%BUILD_DIR%

if not exist "%BUILD_DIR%" (
    echo ERROR: build directory not found: "%BUILD_DIR%"
    goto :fail
)

echo [1/3] Building project...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto :fail

if not exist "%BUILD_DIR%\Release\%APP_EXE%" (
    echo ERROR: executable not found: "%BUILD_DIR%\Release\%APP_EXE%"
    goto :fail
)

echo [2/3] Building installer...
"%ISCC%" "%SCRIPT_DIR%BuildInstaller.iss"
if errorlevel 1 goto :fail

echo [3/3] Done.
echo Installer created under installer\output
goto :eof

:fail
echo.
echo FAILED. Please check the error messages above.
exit /b 1
