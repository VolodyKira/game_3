@echo off
setlocal EnableExtensions DisableDelayedExpansion
title Game3 Userscript Setup
set "GAME3_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if /i "%~1"=="/remove" goto remove
if not "%~1"=="" goto usage
if not defined LOCALAPPDATA goto failed
if not exist "%~dp0Game3-Updater-win64.exe" goto missing

echo Installing Game3 Userscript Updater for the current Windows user.
echo It checks GitHub once at sign-in and exits. No browser settings are changed.
set "GAME3_HOME=%LOCALAPPDATA%\Game3Updater"
set "GAME3_SOURCE=%~dp0Game3-Updater-win64.exe"
"%GAME3_PS%" -NoProfile -NonInteractive -Command "$ErrorActionPreference='Stop'; try { $task='Game3 Userscript Update'; $owner=[System.Security.Principal.WindowsIdentity]::GetCurrent().Name; $dir=$env:GAME3_HOME; $exe=Join-Path $dir 'Game3-Updater-win64.exe'; New-Item -ItemType Directory -Path $dir -Force | Out-Null; if ($env:GAME3_SOURCE -ine $exe) { Copy-Item -LiteralPath $env:GAME3_SOURCE -Destination $exe -Force }; $action=New-ScheduledTaskAction -Execute $exe -Argument '--scheduled' -WorkingDirectory $dir; $trigger=New-ScheduledTaskTrigger -AtLogOn -User $owner; $trigger.Delay='PT45S'; $principal=New-ScheduledTaskPrincipal -UserId $owner -LogonType Interactive -RunLevel Limited; $settings=New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 2) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries; Register-ScheduledTask -TaskName $task -TaskPath '\' -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'Downloads the Game3 userscript from GitHub when changed, then exits.' -Force | Out-Null; try { Start-ScheduledTask -TaskName $task -TaskPath '\' -ErrorAction Stop } catch { Write-Warning 'Could not start the first check immediately; the sign-in task is registered.' }; exit 0 } catch { Write-Error $_ -ErrorAction Continue; exit 1 }"
if errorlevel 1 goto failed

echo Setup complete. Opening the userscript page. If Tampermonkey prompts, click Install.
start "" "https://raw.githubusercontent.com/VolodyKira/game_3/refs/heads/arena/27e65b2f-game-3/games-redirect.user.js"
exit /b 0

:remove
"%GAME3_PS%" -NoProfile -NonInteractive -Command "$ErrorActionPreference='Stop'; try { Unregister-ScheduledTask -TaskName 'Game3 Userscript Update' -TaskPath '\' -Confirm:$false; exit 0 } catch { Write-Error $_ -ErrorAction Continue; exit 1 }"
if errorlevel 1 goto failed
echo Sign-in check removed. Local files and Tampermonkey were not changed.
exit /b 0

:missing
echo Extract the entire ZIP before running this batch file. The EXE must be next to it.
goto failed

:usage
echo Run Setup-Game3.bat without arguments to set up, or use /remove to unregister the task.
exit /b 2

:failed
echo Setup did not complete. See the message above.
pause
exit /b 1
