// api/tasas/manual-clear.js
import { getRedisClient } from "../lib/redis";

const MANUAL_KEY = "rates:bcv:manual";

export default async function handler(req, res) {
  if (req.method !== "POST") {
    return res.status(405).json({ error: "Method not allowed" });
  }

  try {
    const redis = await getRedisClient();
    await redis.del(MANUAL_KEY);

    return res.status(200).json({ ok: true });
  } catch (err) {
    console.error("manual clear error:", err);
    return res.status(500).json({ error: "Failed to clear manual rates" });
  }
}