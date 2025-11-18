// api/tasas.js

export default async function handler(req, res) {
  try {
    // USD BCV
    const bc = await fetch("https://ve.dolarapi.com/v1/dolares/oficial");
    const usd_json = await bc.json();

    const usd_rate  = usd_json.promedio.toFixed(2);
    const fecha     = usd_json.fechaActualizacion;

    // EUR vía Yahoo Finance (EURVEF)
    const r2 = await fetch("https://query1.finance.yahoo.com/v8/finance/chart/EURVES=X");
    const eur_json = await r2.json();

    const eur_rate = eur_json.chart.result[0].meta.regularMarketPrice.toFixed(2);

    return res.status(200).json({
      usd: usd_rate,
      eur: eur_rate,
      fecha: fecha
    });

  } catch (error) {
    console.error("API ERROR:", error);
    return res.status(500).json({ error: "API error" + error });
  }
}
