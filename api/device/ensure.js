import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  try {
    const { deviceId } = req.query;
    if (!deviceId) return res.status(400).json({ error: "deviceId requerido" });

    const redis = await getRedisClient();

    // Ver si existe
    const fw = await redis.hGetAll(`fw:${deviceId}`);
    const paid = await redis.get(`paid:${deviceId}`);

    if (Object.keys(fw).length === 0) {
      // Crear configuración base
      await redis.hSet(`fw:${deviceId}`, {
        version: "1.0.0",
        url: "",
        force: "false",
        notes: "Dispositivo recién creado"
      });

      await redis.set(`paid:${deviceId}`, "false");

      return res.status(200).json({
        deviceId,
        created: true,
        paid: false,
        version: "1.0.0",
        url: ""
      });
    }

    // Ya existe
    return res.status(200).json({
      deviceId,
      created: false,
      paid: paid === "true",
      ...fw
    });

  } catch (e) {
    console.error(e);
    res.status(500).json({ error: "Server error" });
  }
}