@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "NODE=%~1"
set "PORT=%~2"
if "%CRYSTAL%"=="" set "CRYSTAL=8mhz"
if /i "%CRYSTAL%"=="8mhz" (
  set "ENV=megaatmega2560"
) else if /i "%CRYSTAL%"=="16mhz" (
  set "ENV=megaatmega2560-16mhz"
) else (
  echo CRYSTAL must be 8mhz or 16mhz.
  goto :fail
)

echo Mazduino CAN IO Extender - firmware flash
echo.

if exist platformio.ini goto :source

set "AVRDUDE=%~dp0tools\avrdude\windows\avrdude.exe"
set "CONF=%~dp0tools\avrdude\windows\avrdude.conf"
if not exist "%AVRDUDE%" (
  echo avrdude.exe missing from tools\avrdude\windows. Extract the whole zip again.
  goto :fail
)

echo Serial ports found:
powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object { '  ' + $_ }"
echo.
if "%PORT%"=="" set /p "PORT=Port (e.g. COM5): "
if "%PORT%"=="" (echo No port given. & goto :fail)
if "%NODE%"=="" set /p "NODE=Node 0-3 [0]: "
if "%NODE%"=="" set "NODE=0"
echo %NODE%| findstr /r "^[0-3]$" >nul || (echo Node must be 0-3. & goto :fail)

set "HEX=hex\can-io-extender-%CRYSTAL%-node%NODE%.hex"
if not exist "%HEX%" (echo %HEX% not found. & goto :fail)

echo.
echo File: %HEX%
echo Port: %PORT%
echo Disconnect 12 V from the module; keep only USB connected.
pause

"%AVRDUDE%" -C "%CONF%" -p atmega2560 -c wiring -P %PORT% -b 115200 -D -U flash:w:"%HEX%":i
if errorlevel 1 (
  echo.
  echo No sync. Press RESET on the Mega just as avrdude starts, then try again.
  goto :fail
)
goto :done

:source
if "%NODE%"=="" set "NODE=0"
echo %NODE%| findstr /r "^[0-3]$" >nul || (echo Node must be 0-3. & goto :fail)
set "PIO="
where pio >nul 2>nul && set "PIO=pio"
if not defined PIO if exist "%USERPROFILE%\.platformio\penv\Scripts\pio.exe" set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
if not defined PIO (
  echo PlatformIO not found: pip install platformio
  goto :fail
)
set "UPLOAD_PORT="
if not "%PORT%"=="" set "UPLOAD_PORT=--upload-port %PORT%"
echo Build %ENV%, node %NODE% %PORT%
echo Disconnect 12 V from the module; keep only USB connected.
pause
set "PLATFORMIO_BUILD_FLAGS=-D NODE_ID=%NODE%"
"%PIO%" run -e %ENV% -t upload %UPLOAD_PORT%
if errorlevel 1 goto :fail

:done
echo.
echo Done. Unplug USB, then reconnect 12 V.
pause
exit /b 0

:fail
echo.
echo Flash failed.
pause
exit /b 1
