# ConsultorPrecios.Api — Guía de Integración

API REST mínima (.NET 8) para consultar precios de productos desde cualquier ERP con SQL Server.  
Se ejecuta como **Servicio de Windows** y expone un endpoint HTTP.

---

## Instalación rápida

### 1. Publicar
```bat
publicar.bat
```

### 2. Configurar `appsettings.json` en la carpeta `publish/`
Edite la cadena de conexión y el query SQL según su ERP (ver sección abajo).

### 3. Instalar el servicio
```bat
instalar-servicio.bat   ← ejecutar como Administrador
```

---

## Configuración del Query Dinámico

El query se define en `appsettings.json` bajo `Consultor:ProductQuery`.

### Placeholders disponibles

| Placeholder      | Se reemplaza por                        | Ejemplo      |
|------------------|-----------------------------------------|--------------|
| `{PriceColumn}`  | `{PriceColumnPrefix}` + número de tipo  | `Precio3`    |
| `@code`          | Parámetro SQL (código del producto)     | `'ABC-001'`  |

> ⚠️ **Seguridad**: `@code` siempre se envía como parámetro parametrizado. Nunca se inserta directamente en el SQL.

### Columnas que DEBE retornar el query

| Columna   | Tipo            | Descripción                                 |
|-----------|-----------------|---------------------------------------------|
| `Descrip` | string          | Nombre / descripción del producto           |
| `Precio`  | decimal / float | Precio base (sin impuesto)                  |
| `Monto`   | decimal / float | Porcentaje de impuesto (ej: 16). Puede NULL |

---

## Ejemplos por ERP

### Saint Enterprise (por defecto)
```json
"ProductQuery": "SELECT TOP 1 p.Descrip, p.{PriceColumn} AS Precio, (SELECT Monto FROM sataxprd WHERE CodProd = p.CodProd) AS Monto FROM saprod p INNER JOIN sacodbar b ON b.CodProd = p.CodProd WHERE p.CodProd = @code OR b.CodAlte = @code"
```

### ERP genérico con una sola tabla
```json
"ProductQuery": "SELECT TOP 1 Nombre AS Descrip, {PriceColumn} AS Precio, Impuesto AS Monto FROM Productos WHERE Codigo = @code OR CodigoAlt = @code"
```

### ERP sin impuesto (precio fijo, sin listas)
```json
"ProductQuery": "SELECT TOP 1 Descripcion AS Descrip, PrecioVenta AS Precio, 0 AS Monto FROM Inventario WHERE CodItem = @code",
"PriceColumnPrefix": "PrecioVenta",
"MinPriceType": 1,
"MaxPriceType": 1
```

### ERP con vista ya preparada
```json
"ProductQuery": "SELECT TOP 1 Descrip, {PriceColumn} AS Precio, Iva AS Monto FROM vw_ConsultaPrecios WHERE CodBarra = @code OR CodInterno = @code"
```

---

## Uso de la API

### Consultar producto
```
GET /api/product?code=ABC-001
GET /api/product?code=ABC-001&type=2
```

### Respuesta exitosa
```json
{
  "ok": true,
  "found": true,
  "code": "ABC-001",
  "name": "ACEITE MOTOR 1L",
  "priceBase": 10.50,
  "taxPercent": 16.0,
  "priceWithTax": 12.18,
  "currency": "Bs",
  "message": null
}
```

### Producto no encontrado
```json
{
  "ok": true,
  "found": false,
  "code": "XYZ",
  "message": "PRODUCTO NO ENCONTRADO"
}
```

### Ver configuración activa
```
GET /api/config
```

### Health check
```
GET /health
```

### Swagger UI
```
http://localhost/swagger
```

---

## Desinstalar el servicio
```bat
desinstalar-servicio.bat   ← ejecutar como Administrador
```

---

## Notas
- El servicio corre en el **puerto 80** por defecto. Cámbielo en `Kestrel:Endpoints:Http:Url`.
- Si necesita HTTPS, agregue un endpoint `Https` en la sección `Kestrel`.
- Los logs van al **Visor de Eventos de Windows** bajo `Aplicación`.
