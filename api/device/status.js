import { getRedisClient } from "../../redis.js";

export default async function handler(req, res) {
  try {
    const { deviceId } = req.query;

    if (!deviceId) {
      return res.status(400).json({ error: "deviceId requerido" });
    }

    const redis = await getRedisClient();

    // Intentar leer info del dispositivo (si existe)
    const fw = await redis.hGetAll(`fw:${deviceId}`);
    
    // Leer estado de pago
    const paid = await redis.get(`paid:${deviceId}`);

    res.status(200).json({
      deviceId,
      exists: Object.keys(fw).length > 0,
      paid: paid === "true",
      firmware: fw
    });

  } catch (err) {
    console.error("❌ Error en /api/device/status:", err);
    res.status(500).json({ error: "Error interno del servidor" });
  }
}
