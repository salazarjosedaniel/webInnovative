import * as cheerio from "cheerio";

export default async function handler(req, res) {
  try {
    const html = await fetch("https://www.bcv.org.ve/tasas-cambio").then(r => r.text());

    const $ = cheerio.load(html);

    // USD 👉 busca el elemento oficial
    const usd = $("div.views-field.views-field-field-tasa-del-dolar .field-content")
      .first()
      .text()
      .trim()
      .replace(",", ".");

    // EUR 👉 BCV lo publica en otra sección
    const eur = $("div.views-field.views-field-field-tasa-del-euro .field-content")
      .first()
      .text()
      .trim()
      .replace(",", ".");

    const fecha = new Date().toISOString();

    return res.status(200).json({
      usd,
      eur,
      fecha
    });

  } catch (err) {
    console.error("SCRAPER BCV ERROR:", err);
    return res.status(500).json({ error: "Scraper error" });
  }
}
