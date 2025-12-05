import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  if (req.method !== "POST")
    return res.status(405).json({ error: "Método no permitido" });

  try {
    const { deviceId, firmware, ip } = req.body;

    if (!deviceId)
      return res.status(400).json({ error: "deviceId es obligatorio" });

    const redis = await getRedisClient();

    // Crear si no existe
    await redis.hSet(`fw:${deviceId}`, {
      version: firmware || "unknown",
      url: "https://web-innovative.vercel.app/fw/firmware-" + firmware + '.bin',
      force: "false",
      notes: "Creado automaticamente",
      name:"",
      slogan: "",
      instagram: "",
      tlf: "",
      banco:"",
      rif:""
    });

    // Guardar estado reciente
    await redis.hSet(`device:${deviceId}`, {
      lastSeen: new Date().toISOString(),
      firmware,
      ip
    });

    // Registrar última conexión
    await redis.set(`lastseen:${deviceId}`, Date.now().toString());

    // Registrar online (expira en 90 segundos)
    await redis.set(`online:${deviceId}`, "1", { EX: 90 });

    res.status(200).json({ ok: true });

  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Error interno" });
  }
}
