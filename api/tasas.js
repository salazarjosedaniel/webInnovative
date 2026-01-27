import { getRedisClient } from "../lib/redis";

const CACHE_KEY = "tasas:bcv";
const CACHE_TTL = 12*60; // segundos

export default async function handler(req, res) {
  try {
    const redis = await getRedisClient();

    // 1️⃣ Cache primero
    const cached = await redis.get(CACHE_KEY);
    if (cached) {
      return res.status(200).json(JSON.parse(cached));
    }

    // 2️⃣ Fetch con timeout
    const ac = new AbortController();
    const timeout = setTimeout(() => ac.abort(), 5000);

    const r = await fetch(
      "https://api.dolarvzla.com/public/exchange-rate",
      {
        headers: {
          "x-dolarvzla-key": process.env.DOLARVZLA_KEY,
        },
        signal: ac.signal,
      }
    );

    clearTimeout(timeout);

    if (!r.ok) {
      throw new Error(`Upstream ${r.status}`);
    }

    const json = await r.json();

    // 3️⃣ Validar estructura
    if (!json?.usd && !json?.current?.usd) {
      throw new Error("Invalid API response");
    }

    // Compatibilidad vieja / nueva
    const src = json.current ?? json;

    const data = {
      usd: Number(src.usd).toFixed(2),
      eur: Number(src.eur).toFixed(2),
      fecha: src.date ?? new Date().toISOString(),
    };

    // 4️⃣ Guardar cache
    await redis.set(CACHE_KEY, JSON.stringify(data), {
      EX: CACHE_TTL,
    });

    // 5️⃣ Registrar PING del dispositivo
    const deviceId = req.headers["x-device-id"];
    if (deviceId) {
      await redis.hSet(`fw:${deviceId}`, {
        lastSeen: new Date().toISOString(),
      });
    }

    return res.status(200).json(data);

  } catch (err) {
    console.error("❌ ERROR /api/tasas:", err);
    return res.status(500).json({ error: "API error" });
  }
}
