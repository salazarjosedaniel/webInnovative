// api/tasas/manual-get.js
import { getRedisClient } from "../lib/redis";

const MANUAL_KEY = "rates:bcv:manual";

export default async function handler(req, res) {
  try {
    const redis = await getRedisClient();
    const raw = await redis.get(MANUAL_KEY);

    if (!raw) return res.status(200).json({ exists: false });

    return res.status(200).json({
      exists: true,
      data: JSON.parse(raw),
    });
  } catch (err) {
    return res.status(500).json({ error: "Failed to get manual rate" });
  }
}