import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  if (req.method !== "POST") {
    return res.status(405).json({ error: "Método no permitido" });
  }

  try {
    const { nombre, empresa, email, ciudad, tipo } = req.body;

    if (!nombre || !empresa || !email) {
      return res.status(400).json({ error: "Faltan datos obligatorios" });
    }

    const redis = await getRedisClient();

    const id = Date.now(); // ID simple basado en timestamp

    await redis.hSet(`lead:demo:${id}`, {
      nombre,
      empresa,
      email,
      ciudad: ciudad || "",
      tipo: tipo || "",
      createdAt: new Date().toISOString(),
    });

    // opcional: contador total
    await redis.incr("lead:demo:count");

    res.status(200).json({ ok: true });

  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Error interno" });
  }
}
