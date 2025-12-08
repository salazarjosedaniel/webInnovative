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

        // leer registro existente (si existe)
        const existing = await redis.hGetAll(`fw:${deviceId}`);
        const isNew = Object.keys(existing).length === 0;

        // crear objeto actualizado
        const updated = {
            version: firmware || existing.version || "unknown",
            url: `https://web-innovative.vercel.app/fw/firmware-${firmware}.bin`,
            force: existing.force || "false",
            notes: existing.notes || (isNew ? "Creado automáticamente" : ""),
            // estos campos SOLO se escriben si están vacíos antes
            name: existing.name || "",
            slogan: existing.slogan || "",
            instagram: existing.instagram || "",
            tlf: existing.tlf || "",
            banco: existing.banco || "",
            rif: existing.rif || "",
            lastSeen: new Date().toISOString(),
            ip: ip || existing.ip || ""
        };

        // guardar sin borrar datos anteriores
        await redis.hset(`fw:${deviceId}`, updated);

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
