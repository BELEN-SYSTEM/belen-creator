@echo off
REM Prepara la carpeta dist/ para el instalador Inno Setup.
REM Requiere: CMake, Ninja, Qt6 (MSYS2 UCRT64), windeployqt en PATH.

setlocal
cd /d "%~dp0\.."

set BUILD_DIR=build
set DIST_DIR=dist
set QT_PREFIX=%CMAKE_PREFIX_PATH%
if "%QT_PREFIX%"=="" set QT_PREFIX=C:/msys64/ucrt64

REM MSYS2 UCRT64 en PATH para que gcc, moc, ninja encuentren sus DLLs
set "PATH=%QT_PREFIX%\bin;%PATH%"

echo [1/4] Compilando en Release...
cmake -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=Release
cmake --build %BUILD_DIR% --config Release
if errorlevel 1 exit /b 1

echo [2/4] Desplegando dependencias Qt (windeployqt)...
set "WINDEPLOYQT="
if exist "%QT_PREFIX%\bin\windeployqt.exe" set "WINDEPLOYQT=%QT_PREFIX%\bin\windeployqt.exe"
if exist "%QT_PREFIX%\bin\windeployqt6.exe" set "WINDEPLOYQT=%QT_PREFIX%\bin\windeployqt6.exe"
if exist "%QT_PREFIX%\bin\windeployqt-qt6.exe" set "WINDEPLOYQT=%QT_PREFIX%\bin\windeployqt-qt6.exe"
if exist "%QT_PREFIX%\share\qt6\bin\windeployqt.exe" set "WINDEPLOYQT=%QT_PREFIX%\share\qt6\bin\windeployqt.exe"
if "%WINDEPLOYQT%"=="" (
    echo ERROR: windeployqt no encontrado. Instala: pacman -S mingw-w64-ucrt-x86_64-qt6-tools
    exit /b 1
)
"%WINDEPLOYQT%" --release --no-translations --dir %BUILD_DIR% %BUILD_DIR%\Editor.exe
if errorlevel 1 (
    echo ADVERTENCIA: windeployqt fallo.
    exit /b 1
)

REM windeployqt no copia todas las DLLs del prefijo MSYS2; sin ellas Editor.exe falla al arrancar desde build/
echo [2b/4] Copiando DLLs de runtime MSYS2 junto a Editor.exe en %BUILD_DIR%...
for %%d in (libgcc_s_seh-1 libstdc++-6 zlib1 libbrotlidec libbrotlienc libbrotlicommon libzstd libb2-1 libbz2-1 libharfbuzz-0 libfreetype-6 libmd4c libpng16-16 libwinpthread-1 libglib-2.0-0 libgraphite2 libintl-8 libiconv-2 libdouble-conversion) do (
    if exist "%QT_PREFIX%\bin\%%d.dll" copy /y "%QT_PREFIX%\bin\%%d.dll" %BUILD_DIR%\
)
for %%f in ("%QT_PREFIX%\bin\libssl*.dll") do copy /y "%%f" %BUILD_DIR%\
for %%f in ("%QT_PREFIX%\bin\libcrypto*.dll") do copy /y "%%f" %BUILD_DIR%\
for %%f in ("%QT_PREFIX%\bin\libicu*.dll") do copy /y "%%f" %BUILD_DIR%\
for %%f in ("%QT_PREFIX%\bin\libpcre2*.dll") do copy /y "%%f" %BUILD_DIR%\

echo [3/4] Creando carpeta dist...
if exist %DIST_DIR% rmdir /s /q %DIST_DIR%
mkdir %DIST_DIR%
copy /y %BUILD_DIR%\Editor.exe %DIST_DIR%\
copy /y %BUILD_DIR%\*.dll %DIST_DIR%\ 2>nul
for /d %%d in (platforms styles imageformats) do (
    if exist %BUILD_DIR%\%%d xcopy /e /i /y %BUILD_DIR%\%%d %DIST_DIR%\%%d
)
REM DLLs de runtime MSYS2 que Qt usa pero windeployqt no copia
for %%d in (libgcc_s_seh-1 libstdc++-6 zlib1 libbrotlidec libbrotlienc libbrotlicommon libzstd libb2-1 libbz2-1 libharfbuzz-0 libfreetype-6 libmd4c libpng16-16 libwinpthread-1 libglib-2.0-0 libgraphite2 libintl-8 libiconv-2 libdouble-conversion) do (
    if exist "%QT_PREFIX%\bin\%%d.dll" copy /y "%QT_PREFIX%\bin\%%d.dll" %DIST_DIR%\
)
REM OpenSSL para HTTPS (Supabase REST API)
for %%f in ("%QT_PREFIX%\bin\libssl*.dll") do copy /y "%%f" %DIST_DIR%\
for %%f in ("%QT_PREFIX%\bin\libcrypto*.dll") do copy /y "%%f" %DIST_DIR%\
for %%f in ("%QT_PREFIX%\bin\libicu*.dll") do copy /y "%%f" %DIST_DIR%\
for %%f in ("%QT_PREFIX%\bin\libpcre2*.dll") do copy /y "%%f" %DIST_DIR%\
REM TLS backend para QNetworkAccessManager
if exist %BUILD_DIR%\tls xcopy /e /i /y %BUILD_DIR%\tls %DIST_DIR%\tls

echo [4/4] Listo. Directorio dist preparado.
echo Ejecuta: "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss
endlocal
