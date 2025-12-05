import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  try {
    const { deviceId } = req.query;
    const redis = await getRedisClient();

    // Leer config del dispositivo
    const data = await redis.hGetAll(`fw:${deviceId}`);

    // Si no existe, buscar el default
    let firmware = data;
    if (!firmware || Object.keys(firmware).length === 0) {
      firmware = await redis.hGetAll("fw:default");
    }

    // Si tampoco hay default → crear uno básico
    if (!firmware || Object.keys(firmware).length === 0) {
      firmware = {
        version: "1.0.0",
        url: "",
        force: false,
        notes: "",
        name: "",
        slogan: "",
        instagram: "",
        tlf: "",
        banco:"",
        rif: "",
        paid: false
      };
    }

    // Convertir booleanos (Redis siempre guarda strings)
    firmware.force = firmware.force === "true";
    firmware.paid = firmware.paid === "true";

    res.status(200).json({
      deviceId,
      ...firmware,
      timestamp: new Date().toISOString()
    });

  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Error del servidor" });
  }
}
