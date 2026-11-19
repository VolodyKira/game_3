# Run manually in Windows PowerShell. No automatic registration.
[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'Medium')]
param([switch]$Remove)

$ErrorActionPreference = 'Stop'
$taskName = 'Game3 Userscript Update'
$identity = [System.Security.Principal.WindowsIdentity]::GetCurrent()
$owner = $identity.Name

if ($Remove) {
    if ($PSCmdlet.ShouldProcess($taskName, 'Unregister the sign-in task')) {
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
    }
    return
}

$exe = Join-Path $PSScriptRoot 'Game3-Updater-win64.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw 'Place this script next to Game3-Updater-win64.exe first.'
}
$action = New-ScheduledTaskAction -Execute $exe -Argument '--scheduled' -WorkingDirectory $PSScriptRoot
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $owner
# Give the network time to come online. No repeating interval or background service.
$trigger.Delay = 'PT45S'
$principal = New-ScheduledTaskPrincipal -UserId $owner -LogonType Interactive -RunLevel Limited
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 2) `
    -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries

if ($PSCmdlet.ShouldProcess($taskName, "Register one GitHub check at sign-in for $owner")) {
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal `
        -Settings $settings -Description 'Checks for Game3 userscript updates in the public repository, saves the file, and exits.'
}
