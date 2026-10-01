@echo off
setlocal
cd /d "%~dp0"

set "NODE=%~1"
set "PORT=%~2"
if "%NODE%"=="" set "NODE=0"
if "%CRYSTAL%"=="" set "CRYSTAL=8mhz"

echo %NODE%| findstr /r "^[0-3]$" >nul || (echo Node harus 0-3 & goto :fail)

if /i "%CRYSTAL%"=="8mhz" (
  set "ENV=megaatmega2560"
) else if /i "%CRYSTAL%"=="16mhz" (
  set "ENV=megaatmega2560-16mhz"
) else (
  echo CRYSTAL harus 8mhz atau 16mhz
  goto :fail
)

set "PIO="
where pio >nul 2>nul && set "PIO=pio"
if not defined PIO if exist "%USERPROFILE%\.platformio\penv\Scripts\pio.exe" set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
if not defined PIO (
  echo PlatformIO tidak ditemukan. Pasang: pip install platformio
  goto :fail
)

set "UPLOAD_PORT="
if not "%PORT%"=="" set "UPLOAD_PORT=--upload-port %PORT%"

echo Build %ENV% node %NODE% %PORT%
echo Lepas 12 V dari modul sebelum lanjut.
pause

set "PLATFORMIO_BUILD_FLAGS=-D NODE_ID=%NODE%"
"%PIO%" run -e %ENV% -t upload %UPLOAD_PORT%
if errorlevel 1 goto :fail

echo.
echo Selesai. Cabut USB, lalu sambungkan lagi 12 V.
pause
exit /b 0

:fail
echo.
echo Flash gagal.
pause
exit /b 1
