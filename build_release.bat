@echo off
setlocal
set "ROOT=%~dp0."
set "OUT=C:\Users\user\Documents\Codex\2026-10-04\outputs\EldenRingTheaterMode"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CARGO=%USERPROFILE%\.cargo\bin\cargo.exe"
if not exist "%OUT%" mkdir "%OUT%"
"%CMAKE%" -S "%ROOT%" -B "%ROOT%\build" -G "Visual Studio 18 2026" -A x64
if errorlevel 1 exit /b 1
"%CMAKE%" --build "%ROOT%\build" --config Release --parallel
if errorlevel 1 exit /b 1
"%CMAKE%" -S "%ROOT%\probe" -B "%ROOT%\probe\build" -G "Visual Studio 18 2026" -A x64
if errorlevel 1 exit /b 1
"%CMAKE%" --build "%ROOT%\probe\build" --config Release --parallel
if errorlevel 1 exit /b 1
"%CARGO%" build --manifest-path "%ROOT%\adapter\Cargo.toml" --release --locked --offline --target x86_64-pc-windows-msvc
if errorlevel 1 exit /b 1
copy /y "%ROOT%\build\Release\EldenRingTheaterMode.exe" "%OUT%\EldenRingTheaterMode.exe" >nul
if errorlevel 1 exit /b 1
copy /y "%ROOT%\probe\build\Release\EldenRingCompatibilityProbe.exe" "%OUT%\EldenRingCompatibilityProbe.exe" >nul
if errorlevel 1 exit /b 1
copy /y "%ROOT%\adapter\target\x86_64-pc-windows-msvc\release\TheaterMode.dll" "%OUT%\TheaterMode_1_17_diagnostics.dll" >nul
if errorlevel 1 exit /b 1
copy /y "%ROOT%\adapter\target\x86_64-pc-windows-msvc\release\TheaterMode.dll" "%OUT%\TheaterMode.dll" >nul
if errorlevel 1 (
  echo The previous TheaterMode.dll is still loaded. New diagnostic DLL staged as:
  echo %OUT%\TheaterMode_1_17_diagnostics.dll
  exit /b 2
)
echo Release outputs:
echo %OUT%\EldenRingTheaterMode.exe
echo %OUT%\TheaterMode.dll
echo %OUT%\EldenRingCompatibilityProbe.exe
exit /b 0



