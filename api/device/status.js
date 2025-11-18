import { getRedisClient } from "../../redis";

export default async function handler(req, res) {
  try {
    const { deviceId } = req.query;

    if (!deviceId) {
      return res.status(400).json({ error: "deviceId requerido" });
    }

    const redis = await getRedisClient();
    const data = await redis.hGetAll(`fw:${deviceId}`);

    if (!data || Object.keys(data).length === 0) {
      return res.status(404).json({
        deviceId,
        paid: false,
        message: "Dispositivo no registrado"
      });
    }

    return res.status(200).json({
      deviceId,
      paid: data.paid === "true",
      info: data
    });

  } catch (error) {
    console.error("Error en /api/device/status:", error);
    res.status(500).json({ error: "Error consultando estado del dispositivo" });
  }
}