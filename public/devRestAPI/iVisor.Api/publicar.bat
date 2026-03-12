@echo off
:: ════════════════════════════════════════════════════════════════════════════
::  publicar.bat
::  Compila y publica el proyecto listo para instalar como servicio
:: ════════════════════════════════════════════════════════════════════════════

SET PROJECT=iVisor.Api.csproj
SET OUTPUT=%~dp0publish

echo.
echo [INFO] Publicando proyecto...
echo [INFO] Destino: %OUTPUT%
echo.

dotnet publish "%PROJECT%" ^
    -c Release ^
    -r win-x64 ^
    --no-self-contained ^
    -o "%OUTPUT%" ^
    /p:PublishSingleFile=true

IF %ERRORLEVEL% NEQ 0 (
    echo [ERROR] La publicacion fallo. Revise los errores arriba.
    pause
    exit /b 1
)

echo.
echo [OK] Publicacion exitosa en: %OUTPUT%
echo.
echo Pasos siguientes:
echo   1. Copie la carpeta "publish" al servidor destino
echo   2. Edite appsettings.json con la cadena de conexion correcta
echo   3. Ejecute instalar-servicio.bat como Administrador
echo.
pause
