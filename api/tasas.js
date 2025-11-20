import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  try {
    const r = await fetch("https://api.dolarvzla.com/public/exchange-rate");
    const json = await r.json();

    const usd = json.current.usd.toFixed(2);
    const eur = json.current.eur.toFixed(2);
    const fecha = json.current.date;

    // Registrar PING del dispositivo
      const redis = await getRedisClient();
      const deviceId = req.headers["x-device-id"]; // Lo mandaremos desde el ESP32

    if (deviceId) {
       await redis.hSet(`fw:${deviceId}`, {
       lastSeen: new Date().toISOString()
       });
    }

    return res.status(200).json({
      usd,
      eur,
      fecha
    });

  } catch (err) {
    console.error("❌ ERROR /api/tasas:", err);
    return res.status(500).json({ error: "API error" });
  }
}
