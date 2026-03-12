@echo off
:: ════════════════════════════════════════════════════════════════════════════
::  instalar-servicio.bat
::  Instala ConsultorPrecios.Api como Servicio de Windows
::  Ejecutar como ADMINISTRADOR
:: ════════════════════════════════════════════════════════════════════════════

NET SESSION >nul 2>&1
IF %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Este script debe ejecutarse como Administrador.
    pause
    exit /b 1
)

SET SERVICE_NAME=ConsultorPrecios.Api
SET DISPLAY_NAME=Consultor de Precios API
SET DESCRIPTION=API REST para consulta de precios e inventario desde ERP. Puerto 80.
SET EXE_PATH=%~dp0ConsultorPrecios.Api.exe

echo.
echo ════════════════════════════════════════════════
echo   Instalando: %DISPLAY_NAME%
echo   Ejecutable: %EXE_PATH%
echo ════════════════════════════════════════════════
echo.

:: Verificar que el ejecutable existe
IF NOT EXIST "%EXE_PATH%" (
    echo [ERROR] No se encontro el archivo: %EXE_PATH%
    echo Asegurese de haber publicado el proyecto primero.
    echo Comando de publicacion:
    echo   dotnet publish -c Release -r win-x64 --no-self-contained
    pause
    exit /b 1
)

:: Detener y eliminar servicio anterior si existe
sc query "%SERVICE_NAME%" >nul 2>&1
IF %ERRORLEVEL% EQU 0 (
    echo [INFO] Servicio existente encontrado. Deteniendolo...
    sc stop "%SERVICE_NAME%" >nul 2>&1
    timeout /t 3 /nobreak >nul
    sc delete "%SERVICE_NAME%"
    timeout /t 2 /nobreak >nul
    echo [OK] Servicio anterior eliminado.
)

:: Crear el servicio
echo [INFO] Creando servicio de Windows...
sc create "%SERVICE_NAME%" ^
    binPath= "\"%EXE_PATH%\"" ^
    DisplayName= "%DISPLAY_NAME%" ^
    start= auto ^
    obj= "LocalSystem"

IF %ERRORLEVEL% NEQ 0 (
    echo [ERROR] No se pudo crear el servicio.
    pause
    exit /b 1
)

:: Agregar descripción
sc description "%SERVICE_NAME%" "%DESCRIPTION%"

:: Configurar reinicio automático en caso de fallo
sc failure "%SERVICE_NAME%" reset= 86400 actions= restart/5000/restart/10000/restart/30000

:: Iniciar el servicio
echo [INFO] Iniciando servicio...
sc start "%SERVICE_NAME%"

IF %ERRORLEVEL% NEQ 0 (
    echo [ADVERTENCIA] El servicio fue creado pero no pudo iniciarse.
    echo Revise el Visor de Eventos de Windows para mas detalles.
) ELSE (
    echo.
    echo [OK] Servicio instalado e iniciado correctamente.
    echo [OK] API disponible en: http://localhost/api/product?code=CODIGO
    echo [OK] Swagger UI en:     http://localhost/swagger
)

echo.
pause
