import { createClient } from "redis";

const redis = createClient({
  url: process.env.REDIS_URL
});
redis.connect();

export default async function handler(req, res) {
  try {
    if (req.method === "GET") {
      const keys = await redis.keys("fw:*");
      let result = {};

      for (const key of keys) {
        const id = key.replace("fw:", "");
        result[id] = await redis.hGetAll(key);
      }

      return res.status(200).json(result);
    }

    // Crear nuevo dispositivo
    if (req.method === "POST") {
      const id = req.query.device || req.url.split("/").pop();
      await redis.hSet(`fw:${id}`, {
        version: "1.0.0",
        url: "",
        force: "false",
        notes: ""
      });
      return res.status(200).json({ ok: true });
    }

    // Actualizar firmware de un equipo
    if (req.method === "PUT") {
      const id = req.query.device || req.url.split("/").pop();
      const body = JSON.parse(req.body);

      await redis.hSet(`fw:${id}`, {
        version: body.version,
        url: body.url,
        force: body.force,
        notes: body.notes
      });

      return res.status(200).json({ ok: true });
    }

    return res.status(405).send("Method Not Allowed");

  } catch (err) {
    console.error(err);
    return res.status(500).json({ error: err.toString() });
  }
}