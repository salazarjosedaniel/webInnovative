import express from "express";
import fetch from "node-fetch";

const app = express();
const PORT = process.env.PORT || 3000;

app.get("/api/tasas", async (req, res) => {
  try {
    // USD BCV (Fuente confiable)
    const r1 = await fetch("https://ve.dolarapi.com/v1/dolares/oficial");
    const usd_json = await r1.json();

    // EUR/VEF usando el tipo de cambio BCE vía Yahoo Finance
    const r2 = await fetch("https://query1.finance.yahoo.com/v8/finance/chart/EURVES=X");
    const eur_data = await r2.json();

    const eur_rate = eur_data.chart.result[0].meta.regularMarketPrice;

    const response = {
      usd: usd_json.promedio.toFixed(2),
      eur: eur_rate.toFixed(2),
      fecha: usd_json.fechaActualizacion
    };

    res.json(response);

  } catch (err) {
    console.error("API ERROR:", err);
    res.status(500).json({ error: "API error" });
  }
});

app.listen(PORT, () => console.log("API Running on", PORT));
