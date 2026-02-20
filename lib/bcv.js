import { Agent } from "undici";

const insecureDispatcher = new Agent({
  connect: { rejectUnauthorized: false }, // ⚠️ solo para BCV
});

export async function fetchBCVHtml() {
  const url = "https://www.bcv.org.ve/";

  const res = await fetch(url, {
    dispatcher: insecureDispatcher,
    headers: {
      "User-Agent": "InnovativeSolutionsBot/1.0",
      "Accept": "text/html,application/xhtml+xml",
    },
  });

  if (!res.ok) throw new Error(`BCV upstream ${res.status}`);
  return await res.text();
}

/**
 * Parser "sin dependencias": intenta extraer USD/EUR desde el HTML.
 * BCV suele tener los códigos "USD" y "EUR" cerca de un valor con coma decimal.
 *
 * ⚠️ El HTML puede cambiar. Si se rompe, ajustamos los regex con un HTML real.
 */
export function parseBCVRates(html) {
  const normalize = (s) =>
    (s || "")
      .toString()
      .replace(/\s+/g, " ")
      .trim();

  const cleanNumber = (s) => {
    // BCV usa coma decimal. Ej: "36,72"
    const v = normalize(s).replace(".", "").replace(",", ".");
    // deja solo dígitos y punto
    const m = v.match(/[0-9]+(\.[0-9]+)?/);
    return m ? m[0] : "";
  };

  // Heurística: buscar "USD" y un número cerca.
  // Ejemplo tolerante: USD ... 36,72
  const usdMatch =
    html.match(/USD[\s\S]{0,600}?([0-9]{1,3}(?:\.[0-9]{3})*,[0-9]{2})/i) ||
    html.match(/USD[\s\S]{0,600}?([0-9]{1,3},[0-9]{2})/i);

  const eurMatch =
    html.match(/EUR[\s\S]{0,600}?([0-9]{1,3}(?:\.[0-9]{3})*,[0-9]{2})/i) ||
    html.match(/EUR[\s\S]{0,600}?([0-9]{1,3},[0-9]{2})/i);

  const usd = cleanNumber(usdMatch?.[1] || "");
  const eur = cleanNumber(eurMatch?.[1] || "");

  if (!usd) throw new Error("No pude extraer USD del HTML de BCV");
  // eur puede fallar si el HTML cambió o BCV no lo muestra como esperas
  // pero normalmente debería existir:
  if (!eur) throw new Error("No pude extraer EUR del HTML de BCV");

  // Fecha: BCV a veces muestra "Fecha Valor" con una fecha en texto.
  // Intento extraer algo tipo dd/mm/yyyy o dd-mm-yyyy.
  const dateMatch =
    html.match(/Fecha\s*Valor[\s\S]{0,200}?([0-3]?\d[\/\-][01]?\d[\/\-]\d{4})/i) ||
    html.match(/([0-3]?\d[\/\-][01]?\d[\/\-]\d{4})/);

  const fechaValor = dateMatch?.[1] ? normalize(dateMatch[1]) : null;

  return {
    usd,
    eur,
    fechaValor,
    fetchedAt: new Date().toISOString(),
    provider: "bcv-scrape",
  };
}

export async function fetchAndParseBCV() {
  const html = await fetchBCVHtml();
  return parseBCVRates(html);
}