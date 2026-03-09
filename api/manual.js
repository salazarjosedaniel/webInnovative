// api/tasas/manual.js
import { getRedisClient } from "../lib/redis";

const MANUAL_KEY = "rates:bcv:manual";

export default async function handler(req, res) {
  if (req.method !== "POST") {
    return res.status(405).json({ error: "Method not allowed" });
  }

  try {
    const { usd, eur, fecha } = req.body || {};

    if (!usd || !eur) {
      return res.status(400).json({ error: "usd and eur are required" });
    }

    const redis = await getRedisClient();

    const payload = {
      usd: Number(usd),
      eur: Number(eur),
      fecha: fecha || new Date().toISOString(),
      provider: "manual",
      updatedAt: new Date().toISOString(),
    };

    await redis.set(MANUAL_KEY, JSON.stringify(payload));

    return res.status(200).json({
      ok: true,
      saved: payload,
    });

  } catch (err) {
    console.error("manual tasas error:", err);
    return res.status(500).json({ error: "Failed to save manual rates" });
  }
}