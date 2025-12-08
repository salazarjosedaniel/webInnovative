import { getRedisClient } from "../../lib/redis";

export const config = {
  api: {
    bodyParser: {
      sizeLimit: "1mb"
    }
  }
};

const safe = v => (v === undefined || v === null ? "" : v);

export default async function handler(req, res) {
  if (req.method !== "POST")
    return res.status(405).json({ error: "Método no permitido" });

  try {
    const { deviceId, firmware, ip } = req.body;

    if (!deviceId) {
      console.log("❌ deviceId faltante. Body:", req.body);
      return res.status(400).json({ error: "deviceId es obligatorio" });
    }

    const redis = await getRedisClient();

    // leer si existe
    const existing = await redis.hgetall(`fw:${deviceId}`);
    const isNew = Object.keys(existing).length === 0;

    const fwVersion = safe(firmware) || safe(existing.version) || "1.0.0";

    const updated = {
      version: fwVersion,
      url: `https://web-innovative.vercel.app/fw/firmware-${fwVersion}.bin`,
      force: safe(existing.force) || "false",
      notes: safe(existing.notes) || (isNew ? "Creado automáticamente" : ""),
      name: safe(existing.name),
      slogan: safe(existing.slogan),
      instagram: safe(existing.instagram),
      tlf: safe(existing.tlf),
      banco: safe(existing.banco),
      rif: safe(existing.rif)
    };

    // hset require key-value list, not object
    await redis.hset(
      `fw:${deviceId}`,
      Object.entries(updated).flat()
    );

    // actualiza last seen
    await redis.set(`lastseen:${deviceId}`, Date.now().toString());

    // marcar online por 90s
    await redis.set(`online:${deviceId}`, "1", { EX: 90 });

    res.status(200).json({
      ok: true,
      newDevice: isNew,
      updated
    });

  } catch (err) {
    console.error("🔥 ERROR EN /ensure:", err);
    res.status(500).json({ error: "Error interno del servidor" });
  }
}
