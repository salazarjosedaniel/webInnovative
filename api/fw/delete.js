// api/fw/delete.js
import { getRedisClient } from "../../redis.js";

export default async function handler(req, res) {
  if (req.method !== "POST") {
    res.setHeader("Allow", "POST");
    return res.status(405).json({ error: "Method not allowed" });
  }

  try {
    const { id } = req.body || {};

    if (!id) {
      return res.status(400).json({ error: "El campo 'id' es obligatorio" });
    }

    // Si quieres proteger el default, lo puedes bloquear aquí:
    // if (id === "default") {
    //   return res.status(400).json({ error: "No se puede borrar el default" });
    // }

    const client = await getRedisClient();
    const key = `fw:${id}`;

    await client.del(key);

    return res.status(200).json({ ok: true });
  } catch (err) {
    console.error("Error en /api/fw/delete:", err);
    return res.status(500).json({ error: "Error eliminando firmware" });
  }
}