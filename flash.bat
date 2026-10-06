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

if "%PORT%"=="" call :choose_port
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

for /l %%t in (1,1,3) do (
  if %%t gtr 1 (echo. & echo No sync, retrying %%t/3... & timeout /t 1 /nobreak >nul)
  "%AVRDUDE%" -C "%CONF%" -p atmega2560 -c wiring -P %PORT% -b 115200 -D -U flash:w:"%HEX%":i && goto :done
)
echo.
echo Still no sync. Manual reset:
echo Press a key, then press RESET on the Mega right away.
pause >nul
"%AVRDUDE%" -C "%CONF%" -p atmega2560 -c stk500v2 -P %PORT% -b 115200 -D -U flash:w:"%HEX%":i && goto :done
echo.
echo Check the USB cable and that no other program (TunerStudio, Arduino IDE,
echo a serial monitor) has the port open.
goto :fail

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

:choose_port
set "N=0"
set "LIST=%TEMP%\iox_ports.txt"
powershell -NoProfile -Command "$out = foreach ($d in (Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '\(COM\d+\)' })) { $com = [regex]::Match($d.Name, 'COM\d+').Value; $id = ''; if ($d.DeviceID -match 'VID_([0-9A-F]{4})&PID_([0-9A-F]{4})') { $id = $matches[1] + ':' + $matches[2] }; $label = switch -regex ($id) { '^(2341|2A03):(0042|0010|0242)$' { 'Arduino Mega 2560' } '^(2341|2A03):' { 'Arduino' } '^1A86:' { 'CH340 USB serial (Mega clone)' } '^0403:' { 'FTDI USB serial' } '^10C4:' { 'CP210x USB serial' } default { $d.Name -replace '\s*\(COM\d+\)', '' } }; $rank = 1; if ($label -match 'Mega') { $rank = 0 }; [pscustomobject]@{ r = $rank; c = $com; l = $label } }; $out | Sort-Object r, c | ForEach-Object { $_.c + '|' + $_.l }" > "%LIST%"
for /f "usebackq tokens=1,* delims=|" %%a in ("%LIST%") do (
  set /a N+=1
  set "P!N!=%%a"
  set "L!N!=%%b"
)
if !N!==0 (
  echo No USB serial port found. Check the USB cable.
  pause
  goto :choose_port
)
echo Serial ports:
for /l %%i in (1,1,!N!) do echo   %%i^) !P%%i!    !L%%i!
echo   r^) scan again
set "PICK="
set /p "PICK=Choose port [1]: "
if "!PICK!"=="" set "PICK=1"
if /i "!PICK!"=="r" goto :choose_port
set "PORT="
for /l %%i in (1,1,!N!) do if "!PICK!"=="%%i" set "PORT=!P%%i!"
if "!PORT!"=="" (
  echo Not in the list.
  goto :choose_port
)
exit /b 0
