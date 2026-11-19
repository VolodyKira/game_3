@echo off
setlocal EnableExtensions DisableDelayedExpansion
title Game3 Local Catalogue
set "GAME3_HTML=%~dp0game3-catalogue.html"
if not exist "%GAME3_HTML%" (
  echo Extract this batch file and game3-catalogue.html into the same folder.
  pause
  exit /b 1
)
echo Game3 local catalogue: http://127.0.0.1:8765/game3-catalogue.html
echo Leave this window open while using the local catalogue. Close it to stop.
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -Command "$ErrorActionPreference='Stop'; $ascii=[Text.Encoding]::ASCII; $utf8=[Text.Encoding]::UTF8; $nl=[string][char]13+[char]10; $html=[IO.File]::ReadAllBytes($env:GAME3_HTML); $listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,8765); try { $listener.ExclusiveAddressUse=$true; $listener.Start(8); while ($true) { $client=$listener.AcceptTcpClient(); try { $stream=$client.GetStream(); $stream.ReadTimeout=1500; $stream.WriteTimeout=1500; $buf=New-Object byte[] 4096; $used=0; $req=''; $clock=[Diagnostics.Stopwatch]::StartNew(); while ($used -lt $buf.Length -and $clock.ElapsedMilliseconds -lt 1500 -and -not $req.Contains($nl+$nl)) { $count=$stream.Read($buf,$used,$buf.Length-$used); if ($count -eq 0) { break }; $used+=$count; $req=$ascii.GetString($buf,0,$used) }; if (-not $req.Contains($nl+$nl)) { continue }; $status='404 Not Found'; $type='text/plain'; $body=$utf8.GetBytes('Not found'); if ($req -match '(?im)^Host: 127[.]0[.]0[.]1:8765\r?$') { if ($req -cmatch '\AGET /game3-health HTTP/1[.][01]\r\n') { $status='200 OK'; $body=$ascii.GetBytes('game3-local-catalogue-v1') } elseif ($req -cmatch '\AGET /game3-catalogue[.]html HTTP/1[.][01]\r\n') { $status='200 OK'; $type='text/html'; $body=$html } }; $headers=$ascii.GetBytes(('HTTP/1.1 '+$status+$nl+'Content-Type: '+$type+'; charset=utf-8'+$nl+'Content-Length: '+$body.Length+$nl+'Connection: close'+$nl+'Cache-Control: no-store'+$nl+'X-Content-Type-Options: nosniff'+$nl+$nl)); $stream.Write($headers,0,$headers.Length); $stream.Write($body,0,$body.Length) } catch [IO.IOException] { } finally { $client.Close() } } } catch { Write-Error $_ -ErrorAction Continue; exit 1 } finally { $listener.Stop() }"
if errorlevel 1 (
  echo Could not start the local catalogue. Check whether port 8765 is already in use.
  pause
  exit /b 1
)
