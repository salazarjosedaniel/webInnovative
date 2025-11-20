import fetch from "node-fetch";
import * as cheerio from "cheerio";

export async function getBCV_USD() {
  try {
    const url = "https://bcv.org.ve";
    const res = await fetch(url, { method: "GET" });
    const html = await res.text();

    const $ = cheerio.load(html);

    // Encontrar el div con id="dolar"
    const divDolar = $("#dolar");

    // Buscar el primer valor dentro de .col-sm-6.col-xs-6.centrado
    const rawValue = divDolar
      .find(".col-sm-6.col-xs-6.centrado")
      .first()
      .text()
      .trim()
      .replace(",", ".");

    const monto = parseFloat(rawValue);

    return monto;
  } catch (e) {
    console.error("Error leyendo BCV:", e);
    return null;
  }
}

export async function getBCV_EUR() {
  try {
    const url = "https://bcv.org.ve";
    const res = await fetch(url, { method: "GET" });
    const html = await res.text();

    const $ = cheerio.load(html);

    const divEuro = $("#euro");

    const rawValue = divEuro
      .find(".col-sm-6.col-xs-6.centrado")
      .first()
      .text()
      .trim()
      .replace(",", ".");

    return parseFloat(rawValue);
  } catch (e) {
    console.error("Error leyendo EUR BCV:", e);
    return null;
  }
}
