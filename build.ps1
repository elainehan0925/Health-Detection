$ErrorActionPreference = 'Stop'
$utexTargetExe = Join-Path $PSScriptRoot 'program1\camera_preview.exe'
$utexActive = Get-CimInstance Win32_Process -Filter "Name = 'camera_preview.exe'" | Where-Object { $_.ExecutablePath -eq $utexTargetExe }
if ($utexActive) { throw 'Stop Eye Monitor with Ctrl+C, or close its preview with Q/Esc, before rebuilding.' }
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
& 'C:\msys64\ucrt64\bin\g++.exe' -std=c++17 (Join-Path $PSScriptRoot 'program1\program.cpp') -IC:\msys64\ucrt64\include\opencv4 -LC:\msys64\ucrt64\lib -lopencv_dnn -lopencv_objdetect -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_core -lshell32 -luser32 -o (Join-Path $PSScriptRoot 'program1\camera_preview.exe')
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }



