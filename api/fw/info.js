import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  try {
    const { deviceId } = req.query;

    const redis = await getRedisClient();

    // 1. Buscar firmware específico
    let data = await redis.hGetAll(`fw:${deviceId}`);

    // 2. Si no existe, usar "default"
    if (!data || Object.keys(data).length === 0) {
      data = await redis.hGetAll("fw:default");
    }

    // 3. Si tampoco existe, crear fallback
    if (!data || Object.keys(data).length === 0) {
      data = {
        version: "1.0.0",
        url: "",
        force: "false",
        notes: "Sin firmware registrado"
      };
    }

    res.status(200).json({
      deviceId,
      version: data.version,
      url: data.url,
      force: data.force === "true",
      notes: data.notes,
      timestamp: new Date().toISOString()
    });

  } catch (e) {
    console.error("Error FW:", e);
    res.status(500).json({ error: "Error de servidor leyendo firmware" });
  }
}