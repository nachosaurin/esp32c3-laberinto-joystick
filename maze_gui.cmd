@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "ESP_PY=C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe"

if exist "%ESP_PY%" (
    "%ESP_PY%" "%SCRIPT_DIR%tools\maze_gui.py" %*
) else (
    py "%SCRIPT_DIR%tools\maze_gui.py" %*
)

endlocal
