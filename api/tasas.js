import { getBCV_USD, getBCV_EUR } from "../../lib/bcv";

export default async function handler(req, res) {
  const usd = await getBCV_USD();
  const eur = await getBCV_EUR();

  res.status(200).json({
    usd,
    eur,
    fecha: new Date().toISOString()
  });
}