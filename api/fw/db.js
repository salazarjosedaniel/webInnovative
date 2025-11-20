import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  try {
    const redis = await getRedisClient();

    // GET → listar todos
    if (req.method === "GET") {
      const keys = await redis.keys("fw:*");
      let result = {};

      for (const key of keys) {
        const id = key.replace("fw:", "");
        const fw = await redis.hGetAll(key);
        const paidGlobal = await redis.get(`paid:${id}`);
        const lastseen = await redis.get(`lastseen:${id}`);
        const online = await redis.get(`online:${id}`) === "1";

        result[id] = {
          ...fw,
          paidGlobal: paidGlobal === "true",
          lastseen: lastseen,
          online: online
        };
      }

      return res.status(200).json(result);
    }

    // POST → crear nuevo dispositivo
    if (req.method === "POST") {
      const id = req.query.device || req.url.split("/").pop();

      await redis.hSet(`fw:${id}`, {
        version: "1.0.0",
        url: "",
        force: "false",
        notes: ""
      });

      // crear paid:false por defecto
      await redis.set(`paid:${id}`, "false");

      return res.status(200).json({ ok: true });
    }

    // PUT → actualizar firmware
    if (req.method === "PUT") {
      const id = req.query.device || req.url.split("/").pop();
      const body = JSON.parse(req.body);

      await redis.hSet(`fw:${id}`, {
        version: body.version,
        url: body.url,
        force: body.force,
        notes: body.notes
      });

      // paid debe ir *fuera* del fw
      if (body.paid !== undefined) {
        await redis.set(`paid:${id}`, body.paid);
      }

      return res.status(200).json({ ok: true });
    }

    return res.status(405).send("Method Not Allowed");

  } catch (err) {
    console.error(err);
    return res.status(500).json({ error: err.toString() });
  }
}
