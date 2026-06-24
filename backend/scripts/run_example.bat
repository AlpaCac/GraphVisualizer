@echo off
setlocal

set "ROOT_DIR=%~dp0.."
set "BUILD_DIR=%ROOT_DIR%\build"
set "OUTPUT_DIR=%ROOT_DIR%\examples\generated"

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

"%BUILD_DIR%\Release\topoopt.exe" ^
  --config "%ROOT_DIR%\examples\input\wuli.json" ^
  --output-dir "%OUTPUT_DIR%" ^
  --pop 20 ^
  --gen 5 ^
  --mutation 0.01 ^
  --seed 42
if errorlevel 1 exit /b 1

echo Generated:
echo   %OUTPUT_DIR%\luoji.json
echo   %OUTPUT_DIR%\youhua1.json
echo   %OUTPUT_DIR%\youhua2.json

endlocal
