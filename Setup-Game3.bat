@echo off
setlocal EnableExtensions DisableDelayedExpansion
title Game3 - Tampermonkey Setup
set "SOURCE=https://raw.githubusercontent.com/VolodyKira/game_3/refs/heads/main/games-redirect.user.js"
set "CHROME_STORE=https://chromewebstore.google.com/detail/tampermonkey/dhdgffkkebhmkfjojejmpbldmpobfkfo"
set "EDGE_STORE=https://microsoftedge.microsoft.com/addons/detail/tampermonkey/iikmkjmpaadaobahmlepeloendndfphd"
set "OPERA_STORE=https://addons.opera.com/extensions/details/tampermonkey-beta/"
set "CHOICE=%SystemRoot%\System32\choice.exe"
set "FOUND_ANY="
set "GAME3_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
set "GAME3_SOURCE=%~dp0"
if /i "%~1"=="/remove-server" goto removeServer
if not exist "%~dp0game3-catalogue.html" (
    echo Extract the entire ZIP first.
    pause
    exit /b 1
)

if not exist "%~dp0Game3-Local-Server.exe" (
    echo Extract the entire ZIP first. The local server EXE is missing.
    pause
    exit /b 1
)
echo Game3 - Tampermonkey Setup
call :removeOldTask
"%CHOICE%" /c YN /n /m "Start the localhost catalogue now and automatically at Windows sign-in? [Y/N] "
if errorlevel 2 goto findBrowsers
if not errorlevel 1 exit /b 1
call :installServer
if errorlevel 1 goto serverFailed
:findBrowsers

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
echo Make sure Tampermonkey is enabled and has site access to yandex.ru.
"%CHOICE%" /c RK /n /m "[R] Open the userscript  [K] Skip: "
if not errorlevel 1 exit /b 0
if errorlevel 2 exit /b 0

:script
start "" "%FOUND%" "%SOURCE%"
echo The published version was opened once in %~1.
echo If Tampermonkey shows Install or Update, confirm there. If it only offers Reinstall, you can cancel.
echo If prompted, allow the script to check 127.0.0.1 on this PC.
exit /b 0

:installServer
"%GAME3_PS%" -NoProfile -NonInteractive -Command "$ErrorActionPreference='Stop'; try { $name='Game3 Local Catalogue'; $root='\'; $directory=Join-Path $env:LOCALAPPDATA 'Game3LocalCatalogue'; $exe=Join-Path $directory 'Game3-Local-Server.exe'; $owner=[System.Security.Principal.WindowsIdentity]::GetCurrent().Name; $existing=Get-ScheduledTask -TaskPath $root -TaskName $name -ErrorAction SilentlyContinue; if ($existing -and $existing.State -in @('Running','Queued')) { Stop-ScheduledTask -InputObject $existing }; $session = (Get-Process -Id $PID).SessionId; $owned = @(Get-Process -Name 'Game3-Local-Server' -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq $session -and $_.Path -ieq $exe }); foreach ($process in $owned) { if (-not $process.WaitForExit(1500)) { try { Stop-Process -InputObject $process -Force } catch { if (-not $process.HasExited) { throw } }; if (-not $process.WaitForExit(5000)) { throw 'The installed Game3 server did not exit. No files were copied.' } } }; $deadline = [DateTime]::UtcNow.AddSeconds(5); while ((Get-ScheduledTask -TaskPath $root -TaskName $name -ErrorAction SilentlyContinue).State -in @('Running','Queued')) { if ([DateTime]::UtcNow -ge $deadline) { throw 'Windows did not stop the Game3 task in time. No files were copied.' }; Start-Sleep -Milliseconds 100 }; New-Item -ItemType Directory -Path $directory -Force | Out-Null; foreach ($file in @('Game3-Local-Server.exe','game3-catalogue.html')) { $from=Join-Path $env:GAME3_SOURCE $file; $to=Join-Path $directory $file; if ($from -ine $to) { Copy-Item -LiteralPath $from -Destination $to -Force } }; $action=New-ScheduledTaskAction -Execute $exe -WorkingDirectory $directory; $trigger=New-ScheduledTaskTrigger -AtLogOn -User $owner; $principal=New-ScheduledTaskPrincipal -UserId $owner -LogonType Interactive -RunLevel Limited; $settings=New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit ([TimeSpan]::Zero) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries; Register-ScheduledTask -TaskName $name -TaskPath $root -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'Serves the Game3 catalogue only at 127.0.0.1:8765.' -Force | Out-Null; Enable-ScheduledTask -TaskName $name -TaskPath $root | Out-Null; Start-ScheduledTask -TaskName $name -TaskPath $root; Write-Output 'Game3 sign-in task enabled and start requested.'; exit 0 } catch { Write-Error $_ -ErrorAction Continue; exit 1 }"
exit /b %errorlevel%

:serverFailed
echo Could not set up the localhost server. Check the message above.
pause
exit /b 1

:removeServer
"%CHOICE%" /c YN /n /m "Stop the local catalogue and remove its sign-in task? [Y/N] "
if errorlevel 2 exit /b 0
if not errorlevel 1 exit /b 1
"%GAME3_PS%" -NoProfile -NonInteractive -Command "$ErrorActionPreference='Stop'; try { $task=Get-ScheduledTask -TaskPath '\' -TaskName 'Game3 Local Catalogue' -ErrorAction SilentlyContinue; if ($task) { if ($task.State -eq 'Running') { Stop-ScheduledTask -InputObject $task }; Unregister-ScheduledTask -InputObject $task -Confirm:$false }; Write-Output 'Local catalogue sign-in task removed. No files were deleted.'; exit 0 } catch { Write-Error $_ -ErrorAction Continue; exit 1 }"
exit /b %errorlevel%

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
