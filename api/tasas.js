// api/tasas.js

export default async function handler(req, res) {
  try {
    let usd_rate = null;
    let eur_rate = null;
    let fecha = null;

    // =========================================
    // 1) USD BCV
    // =========================================
    try {
      const bc = await fetch("https://ve.dolarapi.com/v1/dolares/oficial");
      const usd_json = await bc.json();

      if (usd_json && usd_json.promedio) {
        usd_rate = usd_json.promedio.toFixed(2);
        fecha = usd_json.fechaActualizacion;
      } else {
        console.error("❌ DolarAPI: JSON incompleto", usd_json);
      }
    } catch (err) {
      console.error("❌ Error DolarAPI:", err);
    }

    // =========================================
    // 2) EUR (Yahoo Finance)
    // =========================================
    try {
      const r2 = await fetch("https://query1.finance.yahoo.com/v8/finance/chart/EURVES=X");
      const eur_json = await r2.json();

      const price =
        eur_json?.chart?.result?.[0]?.meta?.regularMarketPrice;

      if (price) {
        eur_rate = price.toFixed(2);
      } else {
        console.error("❌ Yahoo Finance JSON incompleto:", eur_json);
      }
    } catch (err) {
      console.error("❌ Error YahooFinance:", err);
    }

    // =========================================
    // Validación final
    // =========================================
    if (!usd_rate || !eur_rate) {
      return res.status(500).json({
        error: "No se pudieron obtener todas las tasas",
        usd: usd_rate,
        eur: eur_rate
      });
    }

    return res.status(200).json({
      usd: usd_rate,
      eur: eur_rate,
      fecha
    });

  } catch (error) {
    console.error("🔥 ERROR GENERAL /api/tasas:", error);
    return res.status(500).json({ error: "API error" });
  }
}
