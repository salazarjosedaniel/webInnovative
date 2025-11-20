// api/dashboard/summary.js
import { getRedisClient } from "../../lib/redis";

export default async function handler(req, res) {
  try {
    const redis = await getRedisClient();

    // 1) Todos los dispositivos (fw:*)
    const fwKeys = await redis.keys("fw:*");

    let total = 0;
    let paidTrue = 0;
    let paidFalse = 0;
    const firmwareUsage = {};   // { '1.0.0': 10, '1.0.1': 3 }
    let online = 0;
    let offline = 0;

    const now = Date.now();
    const OFFLINE_THRESHOLD_MS = 5 * 60 * 1000; // 5 min sin tocar -> offline

    for (const key of fwKeys) {
      total++;
      const deviceId = key.replace("fw:", "");
      const fw = await redis.hGetAll(key);

      // paid global
      const paid = await redis.get(`paid:${deviceId}`);
      if (paid === "true") paidTrue++;
      else paidFalse++;

      // firmware version
      const version = fw.version || "N/D";
      firmwareUsage[version] = (firmwareUsage[version] || 0) + 1;

      // lastSeen para online/offline (si lo estás guardando en ensure/status)
      const lastSeenStr = await redis.get(`device:lastSeen:${deviceId}`);
      if (lastSeenStr) {
        const lastSeen = parseInt(lastSeenStr, 10);
        if (!isNaN(lastSeen) && (now - lastSeen) <= OFFLINE_THRESHOLD_MS) {
          online++;
        } else {
          offline++;
        }
      } else {
        offline++;
      }
    }

    return res.status(200).json({
      totalDevices: total,
      paid: {
        true: paidTrue,
        false: paidFalse
      },
      firmwareUsage,
      status: {
        online,
        offline
      },
      generatedAt: new Date().toISOString()
    });

  } catch (err) {
    console.error("Error summary dashboard:", err);
    return res.status(500).json({ error: "Error interno en dashboard" });
  }
}
