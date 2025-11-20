import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  try {
    const { deviceId, event } = req.body;
    if (!deviceId || !event)
      return res.status(400).json({ error: "Missing fields" });

    const redis = await getRedisClient();

    const ts = new Date().toISOString().replace("T", " ").substring(0, 19);
    const line = `${ts} -> ${event}`;

    await redis.lPush(`log:${deviceId}`, line);
    await redis.lTrim(`log:${deviceId}`, 0, 49); // solo 50 líneas

    return res.status(200).json({ ok: true });

  } catch (e) {
    return res.status(500).json({ error: e.toString() });
  }
}
