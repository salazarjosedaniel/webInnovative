// api/tasas.js
import { getRedisClient } from "../lib/redis";
import { fetchAndParseBCV } from "../lib/bcv";

const CACHE_KEY = "rates:bcv:latest";
const TTL_FRESH = 60000;      // 60 min fresh
const TTL_STALE = 86400;    // 24h stale aceptable

async function getCached(redis) {
  const raw = await redis.get(CACHE_KEY);
  if (!raw) return null;
  try { return JSON.parse(raw); } catch { return null; }
}


export default async function handler(req, res) {
  try {
  
    const redis = await getRedisClient();

    // 1) Si hay cache fresh, responde
    const cached = await getCached(redis);
    if (cached?.usd && cached?.eur && cached?.freshUntil && Date.now() < cached.freshUntil) {
      return res.status(200).json({
        usd: cached.usd,
        eur: cached.eur,
        fecha: cached.fecha || cached.fetchedAt,
        provider: cached.provider,
        source: "cache",
      });
    }

    // 2) Si no hay fresh, intentamos refrescar desde BCV
    const live = await fetchAndParseBCV();

    const payload = {
      usd: live.usd,
      eur: live.eur,
      fecha: live.fechaValor || live.fetchedAt,
      provider: live.provider,
      fetchedAt: live.fetchedAt,
      freshUntil: Date.now() + TTL_FRESH * 1000,
      staleUntil: Date.now() + TTL_STALE * 1000,
    };

    await redis.set(CACHE_KEY, JSON.stringify(payload), { EX: TTL_STALE });

    return res.status(200).json({
      usd: payload.usd,
      eur: payload.eur,
      fecha: payload.fecha,
      provider: payload.provider,
      source: "live",
    });

  } catch (err) {
    // 3) Fallback: devolver “stale” si existe
    try {
      const redis = await getRedisClient();
      const cached = await getCached(redis);

      if (cached?.usd && cached?.eur && cached?.staleUntil && Date.now() < cached.staleUntil) {
        return res.status(200).json({
          usd: cached.usd,
          eur: cached.eur,
          fecha: cached.fecha || cached.fetchedAt,
          provider: cached.provider,
          source: "stale",
          warning: "BCV unavailable; served last known good rates",
        });
      }
    } catch {}

    console.error("tasas error:", err);
    return res.status(502).json({ error: "BCV unavailable and no cached rates" });
  }
}