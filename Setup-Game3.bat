@echo off
setlocal EnableExtensions DisableDelayedExpansion
title Game3 - Tampermonkey Setup
set "SOURCE=https://raw.githubusercontent.com/VolodyKira/game_3/refs/heads/main/games-redirect.user.js"
set "CHROME_STORE=https://chromewebstore.google.com/detail/tampermonkey/dhdgffkkebhmkfjojejmpbldmpobfkfo"
set "EDGE_STORE=https://microsoftedge.microsoft.com/addons/detail/tampermonkey/iikmkjmpaadaobahmlepeloendndfphd"
set "OPERA_STORE=https://addons.opera.com/extensions/details/tampermonkey-beta/"
set "CHOICE=%SystemRoot%\System32\choice.exe"
set "FOUND_ANY="
if not exist "%~dp0game3-catalogue.html" (
    echo Extract both files from the ZIP into the same folder first.
    pause
    exit /b 1
)

echo Game3 - Tampermonkey Setup. No new task or autostart entry will be created.
call :removeOldTask

call :find chrome.exe "Google\Chrome\Application\chrome.exe"
if defined FOUND call :guide "Google Chrome" "%CHROME_STORE%" "chrome://extensions/"
call :find msedge.exe "Microsoft\Edge\Application\msedge.exe"
if defined FOUND call :guide "Microsoft Edge" "%EDGE_STORE%" "edge://extensions/"
call :find brave.exe "BraveSoftware\Brave-Browser\Application\brave.exe"
if defined FOUND call :guide "Brave" "%CHROME_STORE%" "brave://extensions/"
call :find vivaldi.exe "Vivaldi\Application\vivaldi.exe"
if not defined FOUND if exist "%LOCALAPPDATA%\Vivaldi\Application\vivaldi.exe" set "FOUND=%LOCALAPPDATA%\Vivaldi\Application\vivaldi.exe"
if defined FOUND call :guide "Vivaldi" "%CHROME_STORE%" "vivaldi://extensions/"
call :find - "Yandex\YandexBrowser\Application\browser.exe"
if not defined FOUND if exist "%LOCALAPPDATA%\Yandex\YandexBrowser\Application\browser.exe" set "FOUND=%LOCALAPPDATA%\Yandex\YandexBrowser\Application\browser.exe"
if defined FOUND call :guide "Yandex Browser" "%CHROME_STORE%" "browser://extensions/"
call :find opera.exe "Opera\opera.exe"
if not defined FOUND call :find - "Opera\launcher.exe"
if defined FOUND call :guide "Opera" "%OPERA_STORE%" "opera://extensions/"
call :find - "Opera GX\opera.exe"
if not defined FOUND call :find - "Opera GX\launcher.exe"
if defined FOUND call :guide "Opera GX" "%OPERA_STORE%" "opera://extensions/"

if not defined FOUND_ANY (
    echo No supported installation was found. You can still open the script manually:
    echo %SOURCE%
)
echo.
echo Done. This installer does not stay running.
pause
exit /b 0

:find
set "FOUND="
if not "%~1"=="-" for %%H in (HKCU HKLM) do for %%V in (64 32) do for /f "tokens=2,*" %%A in ('reg.exe query "%%H\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\%~1" /ve /reg:%%V 2^>nul') do if /i "%%A"=="REG_SZ" if not defined FOUND if exist "%%~B" set "FOUND=%%~B"
if not defined FOUND for %%D in ("%LOCALAPPDATA%\Programs" "%LOCALAPPDATA%" "%ProgramFiles%" "%ProgramFiles(x86)%" "%ProgramW6432%") do if not defined FOUND if exist "%%~D\%~2" set "FOUND=%%~D\%~2"
exit /b 0

:guide
set "FOUND_ANY=1"
echo.
echo [%~1]
"%CHOICE%" /c SUK /n /m "[S] Set up or fix Tampermonkey  [U] Check script once (already set up)  [K] Skip: "
if errorlevel 3 exit /b 0
if errorlevel 2 goto script
if not errorlevel 1 exit /b 0
"%CHOICE%" /c IHK /n /m "[I] Open extension store  [H] Tampermonkey is already installed  [K] Skip: "
if errorlevel 3 exit /b 0
if errorlevel 2 goto settings
if not errorlevel 1 exit /b 0
start "" "%FOUND%" "%~2"
echo Install Tampermonkey in %~1, then return here.
"%CHOICE%" /c RK /n /m "[R] Tampermonkey is installed  [K] Skip: "
if not errorlevel 1 exit /b 0
if errorlevel 2 exit /b 0

:settings
start "" "%FOUND%" "%~3"
echo Open Tampermonkey's details. Enable Allow User Scripts if the browser offers that switch.
echo Enable Allow access to file URLs for Tampermonkey.
echo Make sure Tampermonkey is enabled and has site access to yandex.ru.
"%CHOICE%" /c RK /n /m "[R] Open the userscript  [K] Skip: "
if not errorlevel 1 exit /b 0
if errorlevel 2 exit /b 0

:script
start "" "%FOUND%" "%SOURCE%"
echo The published version was opened once in %~1.
echo If Tampermonkey shows Install or Update, confirm there. If it only offers Reinstall, you can cancel.
echo For first-time setup, open the companion HTML in this same browser and profile.
"%CHOICE%" /c RK /n /m "[R] Open and register local HTML  [K] Skip: "
if not errorlevel 1 exit /b 0
if errorlevel 2 exit /b 0
start "" "%FOUND%" "%~dp0game3-catalogue.html"
exit /b 0

:removeOldTask
"%SystemRoot%\System32\schtasks.exe" /Query /TN "Game3 Userscript Update" >nul 2>&1
if errorlevel 1 exit /b 0
echo The previous package left a sign-in task named Game3 Userscript Update.
"%CHOICE%" /c YN /n /m "Remove that old task? [Y/N] "
if not errorlevel 1 exit /b 0
if errorlevel 2 exit /b 0
"%SystemRoot%\System32\schtasks.exe" /Delete /TN "Game3 Userscript Update" /F
if errorlevel 1 echo Could not remove the old task. You can remove it in Task Scheduler.
exit /b 0
