import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  try {
    const { deviceId } = req.query;

    const redis = await getRedisClient();
    const logs = await redis.lRange(`log:${deviceId}`, 0, 49);

    res.status(200).json({ deviceId, logs });
  } catch (err) {
    res.status(500).json({ error: err.toString() });
  }
}
