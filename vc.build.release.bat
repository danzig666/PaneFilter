@echo off
setlocal

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "VS_ROOT=C:\Program Files\Microsoft Visual Studio\18\Community"
if not exist "%VS_ROOT%\Common7\Tools\VsDevCmd.bat" set "VS_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\18\Community"

set "DEV_CMD=%VS_ROOT%\Common7\Tools\VsDevCmd.bat"
set "CMAKE_EXE=%VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if not exist "%DEV_CMD%" (
  echo Could not find VsDevCmd.bat
  exit /b 1
)

if not exist "%CMAKE_EXE%" (
  set "CMAKE_EXE=cmake"
)

if not exist "%ROOT%\bin" mkdir "%ROOT%\bin"

call :build_one x86 x64 "%ROOT%\build" "%ROOT%\bin\x86"
if errorlevel 1 exit /b 1

call :build_one x64 x64 "%ROOT%\build64" "%ROOT%\bin\x64"
if errorlevel 1 exit /b 1

echo See result in directory "bin"
exit /b 0

:build_one
set "TARGET_ARCH=%~1"
set "HOST_ARCH=%~2"
set "BUILD_DIR=%~3"
set "INSTALL_DIR=%~4"

call "%DEV_CMD%" -arch=%TARGET_ARCH% -host_arch=%HOST_ARCH%
if errorlevel 1 exit /b 1

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

"%CMAKE_EXE%" -S "%ROOT%" -B "%BUILD_DIR%" -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" -DCMAKE_BUILD_TYPE=Release -G "NMake Makefiles"
if errorlevel 1 exit /b 1

"%CMAKE_EXE%" --build "%BUILD_DIR%" --target install
if errorlevel 1 exit /b 1

exit /b 0
