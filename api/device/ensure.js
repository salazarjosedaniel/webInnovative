import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  if (req.method !== "POST")
    return res.status(405).json({ error: "Método no permitido" });

  try {
    const { deviceId, firmware, ip } = req.body;

    if (!deviceId) {
      return res.status(400).json({ error: "deviceId es obligatorio" });
    }

    const redis = await getRedisClient();

    // 1) Revisar si ya existe
    const existing = await redis.hgetall(`fw:${deviceId}`);
    const isNew = Object.keys(existing).length === 0;

    // 2) Construir objeto base
    const base = {
      version: firmware || existing.version || "1.0.0",
      url: `https://web-innovative.vercel.app/fw/firmware-${firmware}.bin`,
      force: existing.force || "false",
      notes: existing.notes || (isNew ? "Creado automáticamente" : ""),
      name: existing.name || "",
      slogan: existing.slogan || "",
      instagram: existing.instagram || "",
      tlf: existing.tlf || "",
      banco: existing.banco || "",
      rif: existing.rif || "",
      lastSeen: new Date().toISOString(),
      ip: ip || existing.ip || ""
    };

    // 3) Guardar en Redis
    await redis.hset(`fw:${deviceId}`, base);

    // 4) Última conexión
    await redis.set(`lastseen:${deviceId}`, Date.now().toString());

    // 5) Registrar como online 90s
    await redis.set(`online:${deviceId}`, "1", { EX: 90 });

    res.status(200).json({
      ok: true,
      newDevice: isNew,
      device: base
    });

  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Error interno" });
  }
}
