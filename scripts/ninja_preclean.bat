@echo off
REM Intercepta "ninja -t clean" en Windows: si Editor.exe esta en uso, ninja no puede borrarlo.
REM Debe coincidir con la ruta de ninja en CMakePresets.json (CMAKE_MAKE_PROGRAM apunta aqui).
setlocal EnableExtensions
set "REAL_NINJA=C:\msys64\ucrt64\bin\ninja.exe"
echo %*| findstr /i /c:"-t clean" >nul
if not errorlevel 1 (
  echo [ninja_preclean] Cerrando Editor.exe si bloquea el clean...
  taskkill /F /IM Editor.exe 2>nul
  ping -n 2 127.0.0.1 >nul
)
if not exist "%REAL_NINJA%" (
  echo ERROR: ninja no encontrado en %REAL_NINJA%
  exit /b 1
)
"%REAL_NINJA%" %*
exit /b %ERRORLEVEL%
