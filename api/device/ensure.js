import { getRedisClient } from "../../lib/redis";

export const config = {
  api: {
    bodyParser: {
      sizeLimit: "1mb"
    }
  }
};

// Función para evitar undefined
const safe = v => (v === undefined || v === null ? "" : v.toString());

export default async function handler(req, res) {
  if (req.method !== "POST")
    return res.status(405).json({ error: "Método no permitido" });

  try {
    console.log("📩 BODY RECIBIDO:", req.body);

    const { deviceId, firmware, ip } = req.body;

    if (!deviceId) {
      console.log("❌ deviceId faltante en request");
      return res.status(400).json({ error: "deviceId es obligatorio" });
    }

    const redis = await getRedisClient();

    // --- Leer registro existente ---
    const key = `fw:${deviceId}`;
    const existing = await redis.hgetall(key);
    const isNew = Object.keys(existing).length === 0;

    console.log("📌 EXISTING:", existing);

    const fwVersion = safe(firmware) || safe(existing.version) || "1.0.0";

    // --- IMPORTANTE: aseguramos valores planos ---
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
      rif: safe(existing.rif),
      lastSeen: safe(new Date().toISOString()),
      ip: safe(ip) || safe(existing.ip)
    };

    console.log("📝 UPDATED OBJ:", updated);

    // --- Upstash requiere key-value pairs (nunca objeto directo) ---
    const kvArray = [];
    for (const [k, v] of Object.entries(updated)) {
      kvArray.push(k, safe(v));
    }

    console.log("🧱 KV ARRAY:", kvArray);

    await redis.hset(key, kvArray);

    // Última conexión + online
    await redis.set(`lastseen:${deviceId}`, Date.now().toString());
    await redis.set(`online:${deviceId}`, "1", { EX: 90 });

    return res.status(200).json({
      ok: true,
      newDevice: isNew,
      data: updated
    });

  } catch (err) {
    console.error("🔥 ERROR EXACTO:", err);
    return res.status(500).json({ error: "Error interno del servidor" });
  }
}
