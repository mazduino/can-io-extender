@echo off
setlocal
set "REPO=mazduino/can-io-extender"
set "PORT=%~1"
set "TARGET=%~2"
if "%TARGET%"=="" set "TARGET=0"
if "%CRYSTAL%"=="" set "CRYSTAL=8mhz"
set "DIR=%~dp0"
set "AVRDUDE=%DIR%avrdude\avrdude.exe"

if not exist "%AVRDUDE%" (
  echo Mengunduh avrdude...
  powershell -NoProfile -Command "$ErrorActionPreference='Stop'; [Net.ServicePointManager]::SecurityProtocol='Tls12'; Invoke-WebRequest 'https://github.com/avrdudes/avrdude/releases/download/v8.0/avrdude-v8.0-windows-x64.zip' -OutFile \"$env:TEMP\avrdude.zip\"; Expand-Archive -Force \"$env:TEMP\avrdude.zip\" '%DIR%avrdude'"
  if errorlevel 1 goto :fail
)

if /i "%TARGET:~-4%"==".hex" (
  set "HEX=%TARGET%"
) else (
  echo %TARGET%| findstr /r "^[0-3]$" >nul || (echo Node harus 0-3, atau path ke file .hex & goto :fail)
  set "HEX=%TEMP%\can-io-extender-%CRYSTAL%-node%TARGET%.hex"
  echo Mengunduh firmware node %TARGET%...
  powershell -NoProfile -Command "$ErrorActionPreference='Stop'; [Net.ServicePointManager]::SecurityProtocol='Tls12'; Invoke-WebRequest 'https://github.com/%REPO%/releases/latest/download/can-io-extender-%CRYSTAL%-node%TARGET%.hex' -OutFile \"$env:TEMP\can-io-extender-%CRYSTAL%-node%TARGET%.hex\""
  if errorlevel 1 goto :fail
)

if "%PORT%"=="" (
  echo Port COM yang terdeteksi:
  powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames()"
  set /p "PORT=Masukkan port (mis. COM5): "
)

for %%F in ("%HEX%") do (
  set "HEXDIR=%%~dpF"
  set "HEXNAME=%%~nxF"
)

echo.
echo Port: %PORT%
echo File: %HEX%
echo Lepas 12 V dari modul sebelum lanjut.
pause

pushd "%HEXDIR%"
"%AVRDUDE%" -C "%DIR%avrdude\avrdude.conf" -p m2560 -c wiring -P %PORT% -b 115200 -D -U "flash:w:%HEXNAME%:i"
set "RC=%errorlevel%"
popd
if not "%RC%"=="0" goto :fail

echo.
echo Selesai. Cabut USB, lalu sambungkan lagi 12 V.
pause
exit /b 0

:fail
echo.
echo Flash gagal.
pause
exit /b 1
