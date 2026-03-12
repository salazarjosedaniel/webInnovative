# iVisor Android

Puerto del proyecto **iVisor WPF (C#)** a **Android nativo (Kotlin)**.

---

## Arquitectura

```
iVisorAndroid/
├── app/src/main/java/com/innovative/ivisor/
│   ├── model/
│   │   └── Models.kt          ← Producto, Empresa, ConfigConexion
│   ├── network/
│   │   ├── iVisorApi.kt        ← Interfaz Retrofit (equivalente a los SqlCommand)
│   │   └── RetrofitClient.kt   ← Singleton Retrofit
│   ├── data/
│   │   ├── iVisorRepository.kt ← Acceso a datos + mapeo de respuestas
│   │   └── PreferencesManager.kt ← Guarda la config de conexión (DataStore)
│   └── ui/
│       ├── MainViewModel.kt    ← MVVM ViewModel (equivalente al code-behind WPF)
│       ├── MainActivity.kt     ← Pantalla principal del visor
│       └── ConfigActivity.kt   ← Pantalla de configuración de conexión
└── app/src/main/res/
    ├── layout/
    │   ├── activity_main.xml   ← Equivalente a MainWindow.xaml
    │   └── activity_config.xml ← Equivalente a FormConnectToSQLServer
    └── values/
        ├── colors.xml, strings.xml, themes.xml
```

---

## Equivalencias WPF → Android

| WPF (C#)                          | Android (Kotlin)                        |
|-----------------------------------|-----------------------------------------|
| `MainWindow.xaml`                 | `activity_main.xml`                     |
| `MainWindow.xaml.cs → SetData()`  | `MainViewModel.buscarProducto()`        |
| `MainWindow.xaml.cs → SetDataConf()` | `MainViewModel.cargarEmpresa()`      |
| `SqlCommand → SELECT saprod...`   | `iVisorApi.getProducto()` (Retrofit)    |
| `SqlCommand → SELECT saconf`      | `iVisorApi.getEmpresa()` (Retrofit)     |
| `IConfigurationManager`           | `PreferencesManager` (DataStore)        |
| `FormConnectToSQLServer`          | `ConfigActivity`                        |
| `BitmapImage` desde `byte[]`      | `BitmapFactory.decodeByteArray()`       |
| `NumberFormatInfo` venezolano     | `NumberFormat(Locale("es","VE"))`       |

---

## Requisito: Backend API REST

La app Android **no se conecta directamente a SQL Server**. Necesitas un
backend (API REST) que exponga estos 3 endpoints:

### `GET /api/producto?codigo=XXX&tipo=3`
Equivalente al SELECT en `SetData()`:
```sql
SELECT saprod.CodProd, Descrip,
       (SELECT Monto FROM sataxprd WHERE sataxprd.CodProd = saprod.CodProd) AS Monto,
       Precio3 AS Precio,
       (SELECT Imagen FROM SAIPRD WHERE CodProd = saprod.CodProd) AS Imagen
FROM saprod, sacodbar
WHERE sacodbar.codprod = saprod.CodProd
  AND (saprod.CodProd = @codigo OR sacodbar.CodAlte = @codigo)
```
**Respuesta JSON:**
```json
{
  "codProd": "001234",
  "descrip": "ARROZ CAZADOR 1KG",
  "precio": 15.50,
  "montoIva": 16.0,
  "imagenBase64": "iVBORw0KGgo..."
}
```

### `GET /api/empresa`
Equivalente al SELECT en `SetDataConf()`:
```sql
SELECT Descrip, NroSerial, RIF, Imagen, Factor FROM saconf
```
**Respuesta JSON:**
```json
{
  "descrip": "MI EMPRESA C.A.",
  "rif": "J-12345678-9",
  "nroSerial": "12345",
  "factor": 36.50,
  "imagenBase64": "iVBORw0KGgo..."
}
```

### `GET /api/productos/descripciones`
**Respuesta JSON:** `["ARROZ CAZADOR", "ACEITE MAZEITE", ...]`

---

## Opciones de backend sugeridas

- **.NET 8 Minimal API** (más cercano al código original C#)
- **Node.js + Express** con el paquete `mssql`
- **Python + FastAPI** con `pyodbc`

---

## Configuración en la app

Al abrir la app, aparece la pantalla de **Configuración** donde se ingresa:
- **URL del servidor**: `http://192.168.1.10:8080/`
- **Tipo de precio**: Precio1, Precio2 o Precio3
- **Mostrar precio en dólares**: activa/desactiva el Factor de conversión

La configuración se guarda localmente con **DataStore** (equivalente a `General.config`).

---

## Cómo abrir el proyecto

1. Abre **Android Studio** → *Open* → selecciona la carpeta `iVisorAndroid`
2. Espera que Gradle sincronice las dependencias
3. Conecta un dispositivo Android o inicia un emulador (API 26+)
4. Asegúrate de que tu backend esté corriendo y accesible desde el dispositivo
5. Ejecuta la app con ▶️

> **Tip**: Para pruebas locales con emulador, usa `http://10.0.2.2:8080/`
> en lugar de `http://localhost:8080/`
