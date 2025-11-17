import { kv } from "@vercel/kv";

export default async function handler(req, res) {

  if (req.method === "POST") {
    const { id, version, notes } = req.body;

    await kv.hset(`device:${id}`, {
      version,
      notes
    });

    return res.status(200).json({ ok: true });
  }

  if (req.method === "GET") {
    const { id } = req.query;

    const data = await kv.hgetall(`device:${id}`);

    return res.status(200).json(data || {});
  }

  res.status(405).send("Method Not Allowed");
}