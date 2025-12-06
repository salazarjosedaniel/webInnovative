// api/fw/save.js
import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  if (req.method !== "POST") {
    res.setHeader("Allow", "POST");
    return res.status(405).json({ error: "Method not allowed" });
  }

  try {
    const { id, version, url, paid, force, notes, name, slogan, instagram, banco, tlf, rif } = req.body || {};

    if (!id) {
      return res.status(400).json({ error: "El campo 'id' es obligatorio" });
    }

    const client = await getRedisClient();
    const key = `fw:${id}`;

    await client.hSet(key, {
      version: version ?? "",
      url: url ?? "",
      paid: paid ? "true" : "false",
      force: force ? "true" : "false",
      notes: notes ?? "",
      name: name ?? "",
      slogan: slogan ?? "",
      instagram: instagram ?? "",
      banco: banco ?? "",
      tlf: tlf ?? "",
      rif: rif ?? ""
    });

    return res.status(200).json({ ok: true });
  } catch (err) {
    console.error("Error en /api/fw/save:", err);
    return res.status(500).json({ error: "Error guardando firmware" });
  }
}