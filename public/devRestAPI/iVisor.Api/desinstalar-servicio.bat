@echo off
:: ════════════════════════════════════════════════════════════════════════════
::  desinstalar-servicio.bat
::  Desinstala ConsultorPrecios.Api como Servicio de Windows
::  Ejecutar como ADMINISTRADOR
:: ════════════════════════════════════════════════════════════════════════════

NET SESSION >nul 2>&1
IF %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Este script debe ejecutarse como Administrador.
    pause
    exit /b 1
)

SET SERVICE_NAME=ConsultorPrecios.Api

echo.
echo Deteniendo servicio %SERVICE_NAME%...
sc stop "%SERVICE_NAME%" >nul 2>&1
timeout /t 3 /nobreak >nul

echo Eliminando servicio %SERVICE_NAME%...
sc delete "%SERVICE_NAME%"

IF %ERRORLEVEL% EQU 0 (
    echo [OK] Servicio desinstalado correctamente.
) ELSE (
    echo [ERROR] No se pudo eliminar el servicio. Puede que no existiera.
)

echo.
pause
