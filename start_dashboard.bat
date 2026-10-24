@echo off
title AeroSense Mobile App & Dashboard
cd /d "%~dp0"

setlocal enabledelayedexpansion
echo ======================================================
echo    AeroSense: Mobile App & Bluetooth Air Monitor
echo ======================================================
echo.
echo [1] PC Local Address:
echo     http://localhost:8000/dashboard.html
echo.
echo [2] Phone / Mobile Wi-Fi Address:
for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr /i "ipv4"') do (
    set "ip=%%a"
    set "ip=!ip: =!"
    echo     http://!ip!:8000/dashboard.html
)
echo.
echo ------------------------------------------------------
echo  Open the URL above in your phone browser!
echo  Tap "Install App" or "Add to Home Screen" to install.
echo ------------------------------------------------------
echo.

where python >nul 2>nul
if %ERRORLEVEL% equ 0 (
    start http://localhost:8000/dashboard.html
    python -m http.server 8000
    goto end
)

where py >nul 2>nul
if %ERRORLEVEL% equ 0 (
    start http://localhost:8000/dashboard.html
    py -m http.server 8000
    goto end
)

where npx >nul 2>nul
if %ERRORLEVEL% equ 0 (
    start http://localhost:8000/dashboard.html
    npx -y serve -p 8000 .
    goto end
)

:: Fallback using PowerShell built-in HTTP server on any Windows PC
start http://localhost:8000/dashboard.html
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$listener = New-Object System.Net.HttpListener; ^
   $listener.Prefixes.Add('http://*:8000/'); ^
   $listener.Start(); ^
   Write-Host 'Server listening on port 8000 (Press Ctrl+C to stop)'; ^
   while ($listener.IsListening) { ^
     $ctx = $listener.GetContext(); ^
     $req = $ctx.Request; ^
     $res = $ctx.Response; ^
     $file = $req.Url.LocalPath.TrimStart('/'); ^
     if ([string]::IsNullOrEmpty($file)) { $file = 'dashboard.html' }; ^
     if (Test-Path $file) { ^
       $bytes = [System.IO.File]::ReadAllBytes($file); ^
       $res.ContentLength64 = $bytes.Length; ^
       if ($file.EndsWith('.html')) { $res.ContentType = 'text/html' } ^
       elseif ($file.EndsWith('.js')) { $res.ContentType = 'application/javascript' } ^
       elseif ($file.EndsWith('.json')) { $res.ContentType = 'application/json' } ^
       elseif ($file.EndsWith('.svg')) { $res.ContentType = 'image/svg+xml' } ^
       elseif ($file.EndsWith('.css')) { $res.ContentType = 'text/css' }; ^
       $res.OutputStream.Write($bytes, 0, $bytes.Length); ^
     } else { ^
       $res.StatusCode = 404; ^
     }; ^
     $res.OutputStream.Close(); ^
   }"

:end
pause
