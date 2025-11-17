import { redis } from "../../redisClient.js";

export default async function handler(req, res) {
  const { deviceId } = req.query;

  try {
    // 1. Intentar leer versión específica
    const key = `fw:${deviceId}`;
    let firmware = await redis.hGetAll(key);

    // 2. Si no existe, leer default
    if (!firmware || Object.keys(firmware).length === 0) {
      firmware = await redis.hGetAll("fw:default");
    }

    // 3. Si tampoco existe, crear uno básico
    if (!firmware || Object.keys(firmware).length === 0) {
      firmware = {
        version: "1.0.0",
        force: "false",
        url: "",
        notes: "Sin configuración"
      };
    }

    res.status(200).json({
      deviceId,
      ...firmware,
      timestamp: new Date().toISOString()
    });

  } catch (e) {
    res.status(500).json({
      error: "Error de servidor leyendo firmware",
      details: e.message
    });
  }
}