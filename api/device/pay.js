import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  if (req.method !== "POST") {
    return res.status(405).json({ error: "Method not allowed" });
  }

  try {
    const { id, paid } = req.body;

    if (!id) return res.status(400).json({ error: "Missing id" });

    const redis = await getRedisClient();

    await redis.set(`paid:${id}`, paid ? "true" : "false");

    return res.status(200).json({ ok: true });

  } catch (err) {
    console.error(err);
    return res.status(500).json({ error: err.toString() });
  }
}