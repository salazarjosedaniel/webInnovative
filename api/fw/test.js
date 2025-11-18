// api/fw/test.js
import { getRedisClient } from "../../lib/redis";

async function resolveFirmware(deviceId) {
  const client = await getRedisClient();

  // 1. Config específica del dispositivo
  let data = await client.hGetAll(`fw:${deviceId}`);

  // 2. Si no hay nada, usar default
  if (!data || Object.keys(data).length === 0) {
    data = await client.hGetAll("fw:default");
  }

  if (!data || Object.keys(data).length === 0) {
    return null;
  }

  return {
    deviceId,
    version: data.version || "",
    url: data.url || "",
    force: data.force === "true",
    notes: data.notes || "",
    timestamp: new Date().toISOString()
  };
}

export default async function handler(req, res) {
  if (req.method !== "GET") {
    res.setHeader("Allow", "GET");
    return res.status(405).json({ error: "Method not allowed" });
  }

  try {
    const { id } = req.query;
    const deviceId = id || "default";

    const fw = await resolveFirmware(deviceId);

    if (!fw) {
      return res.status(404).json({ error: "No hay firmware configurado" });
    }

    return res.status(200).json(fw);
  } catch (err) {
    console.error("Error en /api/fw/test:", err);
    return res.status(500).json({ error: "Error probando firmware" });
  }
}