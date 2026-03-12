using System.Data;
using System.Data.Common;
using System.Globalization;
using System.Net.Http.Headers;
using System.Text.Json;
using FirebirdSql.Data.FirebirdClient;
using Microsoft.Data.SqlClient;
using MySqlConnector;
using Npgsql;

// ════════════════════════════════════════════════════════════════════════════
//  ConsultorPrecios.Api  —  Minimal API  +  Windows Service
//  Proveedores soportados: SqlServer | MySQL | Firebird | PostgreSQL | Shopify
// ════════════════════════════════════════════════════════════════════════════

var builder = WebApplication.CreateBuilder(args);

// ── Soporte para ejecutarse como Servicio de Windows ────────────────────────
builder.Host.UseWindowsService(options =>
{
    options.ServiceName = builder.Configuration["WindowsService:ServiceName"]
                          ?? "iVisor.Api";
});

// ── HttpClient para Shopify ──────────────────────────────────────────────────
builder.Services.AddHttpClient("shopify");

// ── Swagger ──────────────────────────────────────────────────────────────────
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen(c =>
{
    c.SwaggerDoc("v1", new()
    {
        Title       = "Consultor de Precios API",
        Version     = "v1",
        Description = "API REST para consulta de precios desde cualquier ERP. " +
                      "Soporta SQL Server, MySQL, Firebird, PostgreSQL y Shopify. " +
                      "El proveedor se configura en appsettings.json."
    });
});

// ── CORS ─────────────────────────────────────────────────────────────────────
builder.Services.AddCors(o =>
    o.AddDefaultPolicy(p =>
        p.AllowAnyOrigin()
         .AllowAnyHeader()
         .AllowAnyMethod()));

var app = builder.Build();
app.UseCors();
app.UseSwagger();
app.UseSwaggerUI();

// ── Configuración ─────────────────────────────────────────────────────────────
var cfg    = app.Configuration;
var logger = app.Logger;

// Proveedor de base de datos: SqlServer | MySQL | Firebird | PostgreSQL | Shopify
string dbProvider = cfg["DatabaseProvider"] ?? "SqlServer";

var consultorSection = cfg.GetSection("Consultor");
string pricePrefix = consultorSection["PriceColumnPrefix"] ?? "Precio";
string defaultType = consultorSection["DefaultPriceType"]  ?? "3";
string currency    = consultorSection["Currency"]           ?? "Bs";
int.TryParse(consultorSection["MinPriceType"] ?? "1", out int minType);
int.TryParse(consultorSection["MaxPriceType"] ?? "6", out int maxType);

// ── Configuración exclusiva de Shopify ───────────────────────────────────────
var shopifySection   = cfg.GetSection("Shopify");
string shopifyStore  = shopifySection["StoreUrl"]    ?? "";   // ej: mitienda.myshopify.com
string shopifyToken  = shopifySection["AccessToken"] ?? "";   // Admin API token

// ── Configuración SQL (solo para proveedores de BD) ──────────────────────────
string connStr  = "";
string rawQuery = "";

if (!dbProvider.Equals("Shopify", StringComparison.OrdinalIgnoreCase))
{
    connStr = cfg.GetConnectionString("Default")
        ?? throw new InvalidOperationException("ConnectionStrings:Default no configurado en appsettings.json");

    rawQuery = consultorSection["ProductQuery"]
        ?? throw new InvalidOperationException("Consultor:ProductQuery no configurado en appsettings.json");
}
else
{
    if (string.IsNullOrWhiteSpace(shopifyStore))
        throw new InvalidOperationException("Shopify:StoreUrl no configurado en appsettings.json");
    if (string.IsNullOrWhiteSpace(shopifyToken))
        throw new InvalidOperationException("Shopify:AccessToken no configurado en appsettings.json");
}

logger.LogInformation(
    "iVior.Api iniciado. Proveedor: {Provider}",
    dbProvider);

// ════════════════════════════════════════════════════════════════════════════
//  FACTORY — conexión SQL según proveedor
// ════════════════════════════════════════════════════════════════════════════
DbConnection CreateConnection() => dbProvider.ToLowerInvariant() switch
{
    "mysql"      => new MySqlConnection(connStr),
    "firebird"   => new FbConnection(connStr),
    "postgresql" => new NpgsqlConnection(connStr),
    "sqlserver"  => new SqlConnection(connStr),
    _ => throw new InvalidOperationException(
            $"DatabaseProvider '{dbProvider}' no reconocido. " +
            "Valores válidos: SqlServer | MySQL | Firebird | PostgreSQL | Shopify")
};

void AddCodeParameter(DbCommand cmd, string value)
{
    DbParameter p = cmd.CreateParameter();
    p.ParameterName = "@code";
    p.Value  = value;
    p.DbType = DbType.String;
    p.Size   = 100;
    cmd.Parameters.Add(p);
}

// ════════════════════════════════════════════════════════════════════════════
//  SHOPIFY — consulta producto por barcode o SKU via Admin API
// ════════════════════════════════════════════════════════════════════════════
async Task<ProductResponse> QueryShopify(
    string code, IHttpClientFactory httpFactory, ILogger log)
{
    // Shopify Admin API: buscar variante por barcode, luego por SKU si no aparece
    // Endpoint: GET /admin/api/2024-04/variants.json?barcode=XXX
    //           GET /admin/api/2024-04/variants.json?sku=XXX  (fallback)

    var http = httpFactory.CreateClient("shopify");
    http.BaseAddress = new Uri($"https://{shopifyStore}");
    http.DefaultRequestHeaders.Add("X-Shopify-Access-Token", shopifyToken);
    http.DefaultRequestHeaders.Accept.Add(
        new MediaTypeWithQualityHeaderValue("application/json"));

    JsonDocument? doc = null;
    JsonElement variants = default;

    // ── Intento 1: buscar por barcode ────────────────────────────────────────
    var resp = await http.GetAsync(
        $"/admin/api/2024-04/variants.json?barcode={Uri.EscapeDataString(code)}&limit=1");

    if (resp.IsSuccessStatusCode)
    {
        doc = JsonDocument.Parse(await resp.Content.ReadAsStringAsync());
        variants = doc.RootElement.GetProperty("variants");
    }

    // ── Intento 2: buscar por SKU si no encontró por barcode ─────────────────
    if (doc == null || variants.GetArrayLength() == 0)
    {
        resp = await http.GetAsync(
            $"/admin/api/2024-04/variants.json?sku={Uri.EscapeDataString(code)}&limit=1");

        if (resp.IsSuccessStatusCode)
        {
            doc?.Dispose();
            doc = JsonDocument.Parse(await resp.Content.ReadAsStringAsync());
            variants = doc.RootElement.GetProperty("variants");
        }
    }

    if (doc == null || variants.GetArrayLength() == 0)
    {
        return new ProductResponse(
            ok: true, found: false, code: code,
            name: null, priceBase: null, taxPercent: null,
            priceWithTax: null, currency: currency,
            stock: null, message: "PRODUCTO NO ENCONTRADO",
            imagenBase64: null);
    }

    var variant = variants[0];

    // ── Precio ───────────────────────────────────────────────────────────────
    double priceBase = 0;
    if (variant.TryGetProperty("price", out var priceProp))
        double.TryParse(priceProp.GetString(), NumberStyles.Any,
            CultureInfo.InvariantCulture, out priceBase);

    // ── Stock ────────────────────────────────────────────────────────────────
    int? stock = null;
    if (variant.TryGetProperty("inventory_quantity", out var stockProp))
        stock = stockProp.GetInt32();

    // ── Nombre del producto (necesita llamada al producto padre) ─────────────
    string productTitle = "";
    string variantTitle = "";

    if (variant.TryGetProperty("title", out var vt))
        variantTitle = vt.GetString() ?? "";

    if (variant.TryGetProperty("product_id", out var pidProp))
    {
        long productId = pidProp.GetInt64();
        var prodResp = await http.GetAsync(
            $"/admin/api/2024-04/products/{productId}.json?fields=title");

        if (prodResp.IsSuccessStatusCode)
        {
            using var prodDoc = JsonDocument.Parse(
                await prodResp.Content.ReadAsStringAsync());
            if (prodDoc.RootElement.TryGetProperty("product", out var prod) &&
                prod.TryGetProperty("title", out var titleProp))
                productTitle = titleProp.GetString() ?? "";
        }
    }

    // Si la variante tiene título propio (ej: "Rojo / L") lo concatenamos
    string fullName = variantTitle is "Default Title" or ""
        ? productTitle
        : $"{productTitle} - {variantTitle}";

    // ── Shopify no devuelve % de impuesto directamente;
    //    se usa taxable flag + tasa configurada en appsettings ─────────────────
    bool taxable = variant.TryGetProperty("taxable", out var taxProp) && taxProp.GetBoolean();
    double.TryParse(shopifySection["DefaultTaxPercent"] ?? "0",
        NumberStyles.Any, CultureInfo.InvariantCulture, out double taxPct);

    double effectiveTax   = taxable ? taxPct : 0;
    double priceWithTax   = priceBase * (1 + effectiveTax / 100);

    doc.Dispose();

    return new ProductResponse(
        ok:           true,
        found:        true,
        code:         code,
        name:         fullName,
        priceBase:    Math.Round(priceBase,     2, MidpointRounding.AwayFromZero),
        taxPercent:   Math.Round(effectiveTax,  2, MidpointRounding.AwayFromZero),
        priceWithTax: Math.Round(priceWithTax,  2, MidpointRounding.AwayFromZero),
        currency:     currency,
        stock:        stock,
        message:      null,
        imagenBase64: null);
}

// ════════════════════════════════════════════════════════════════════════════
//  ENDPOINTS
// ════════════════════════════════════════════════════════════════════════════

// Health-check
app.MapGet("/health", () =>
    Results.Ok(new
    {
        ok         = true,
        service    = "ConsultorPrecios.Api",
        dbProvider = dbProvider,
        timestamp  = DateTime.UtcNow
    })
).WithName("HealthCheck");


// ── GET /api/product ──────────────────────────────────────────────────────────
//  Parámetros:
//    code  (requerido) → código de barras, SKU o código interno
//    type  (opcional)  → lista de precio (solo aplica para proveedores SQL)
app.MapGet("/api/product", async (string code, string? type,
    IHttpClientFactory httpFactory) =>
{
    if (string.IsNullOrWhiteSpace(code))
        return Results.BadRequest(new { ok = false, error = "El parámetro 'code' es requerido." });

    // ── Rama Shopify ──────────────────────────────────────────────────────────
    if (dbProvider.Equals("Shopify", StringComparison.OrdinalIgnoreCase))
    {
        logger.LogInformation("Consultando producto [{Code}] via [Shopify]", code.Trim());
        try
        {
            var result = await QueryShopify(code.Trim(), httpFactory, logger);
            return Results.Ok(result);
        }
        catch (Exception ex)
        {
            logger.LogError(ex, "Error al consultar Shopify para [{Code}]", code);
            return Results.Problem(
                detail: ex.Message,
                title:  "Error al consultar Shopify",
                statusCode: 500);
        }
    }

    // ── Rama SQL ──────────────────────────────────────────────────────────────
    string priceType = string.IsNullOrWhiteSpace(type) ? defaultType : type.Trim();

    if (!int.TryParse(priceType, out int priceTypeInt) || priceTypeInt < minType || priceTypeInt > maxType)
        return Results.BadRequest(new
        {
            ok    = false,
            error = $"Tipo de precio inválido. Debe ser un número entre {minType} y {maxType}."
        });

    string priceColumn = $"{pricePrefix}{priceTypeInt}";
    string sql = rawQuery.Replace("{PriceColumn}", priceColumn);

    logger.LogInformation(
        "Consultando producto [{Code}] tipo precio [{Type}] via [{Provider}]",
        code.Trim(), priceColumn, dbProvider);

    try
    {
        await using DbConnection cn = CreateConnection();
        await cn.OpenAsync();

        await using DbCommand cmd = cn.CreateCommand();
        cmd.CommandText = sql;
        AddCodeParameter(cmd, code.Trim());

        await using DbDataReader rd = await cmd.ExecuteReaderAsync();

        if (!await rd.ReadAsync())
        {
            return Results.Ok(new ProductResponse(
                ok: true, found: false, code: code,
                name: "PRODUCTO NO ENCONTRADO", priceBase: 1,
                taxPercent: 16, priceWithTax: 1.16,
                currency: currency, stock: null,
                message: "PRODUCTO NO ENCONTRADO",
                imagenBase64: null));
        }

        string name  = rd["Descrip"]?.ToString() ?? "";
        double price = Convert.ToDouble(rd["Precio"], CultureInfo.InvariantCulture);
        double tax   = rd["Monto"] == DBNull.Value
                       ? 0
                       : Convert.ToDouble(rd["Monto"], CultureInfo.InvariantCulture);

        // Stock es opcional en proveedores SQL (columna "Stock" si existe)
        int? stock = null;
        try {
            var stockOrd = rd.GetOrdinal("Stock");
            if (!rd.IsDBNull(stockOrd)) stock = rd.GetInt32(stockOrd);
        } catch { /* columna Stock no existe en el query, se ignora */ }

        double priceWithTax = price * (1 + tax / 100);

        return Results.Ok(new ProductResponse(
            ok:           true,
            found:        true,
            code:         code,
            name:         name,
            priceBase:    Math.Round(price,        2, MidpointRounding.AwayFromZero),
            taxPercent:   Math.Round(tax,          2, MidpointRounding.AwayFromZero),
            priceWithTax: Math.Round(priceWithTax, 2, MidpointRounding.AwayFromZero),
            currency:     currency,
            stock:        stock,
            message:      null,
            imagenBase64 : null
            ));
    }
    catch (Exception ex)
    {
        logger.LogError(ex, "Error al consultar producto [{Code}] con proveedor [{Provider}]",
            code, dbProvider);
        return Results.Problem(
            detail: ex.Message,
            title:  "Error al consultar la base de datos",
            statusCode: 500);
    }

}).WithName("GetProduct");


// ── GET /api/config ───────────────────────────────────────────────────────────
app.MapGet("/api/config", () =>
    Results.Ok(new
    {
        ok                = true,
        dbProvider        = dbProvider,
        priceColumnPrefix = pricePrefix,
        defaultPriceType  = defaultType,
        minPriceType      = minType,
        maxPriceType      = maxType,
        currency          = currency,
        queryTemplate     = dbProvider.Equals("Shopify", StringComparison.OrdinalIgnoreCase)
                            ? "(Shopify — no aplica query SQL)"
                            : rawQuery,
        shopifyStore      = dbProvider.Equals("Shopify", StringComparison.OrdinalIgnoreCase)
                            ? shopifyStore
                            : null
    })
).WithName("GetConfig");


app.Run();


// ════════════════════════════════════════════════════════════════════════════
//  MODELOS
// ════════════════════════════════════════════════════════════════════════════

record ProductResponse(
    bool    ok,
    bool    found,
    string  code,
    string? name,
    double? priceBase,
    double? taxPercent,
    double? priceWithTax,
    string  currency,
    int?    stock,
    string? message,
    string? imagenBase64
);
