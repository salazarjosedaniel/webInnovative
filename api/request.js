import { getRedisClient } from "../lib/redis";
import crypto from "crypto";

export default async function handler(req, res) {
  if (req.method !== "POST") {
    return res.status(405).json({ error: "Método no permitido" });
  }

  try {
    const { nombre, empresa, email, ciudad, tipo } = req.body ?? {};

    if (!nombre || !empresa || !email) {
      return res.status(400).json({ error: "Faltan datos obligatorios" });
    }

    // Validación básica de email
    const emailOk = /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(String(email).trim());
    if (!emailOk) {
      return res.status(400).json({ error: "Email inválido" });
    }

    const redis = await getRedisClient();

    // ID robusto y único (evita colisiones si entran 2 leads al mismo ms)
    const id = `${Date.now()}-${crypto.randomBytes(4).toString("hex")}`;

    await redis.hSet(`lead:demo:${id}`, {
      nombre: String(nombre).trim(),
      empresa: String(empresa).trim(),
      email: String(email).trim().toLowerCase(),
      ciudad: ciudad ? String(ciudad).trim() : "",
      tipo: tipo ? String(tipo).trim() : "",
      createdAt: new Date().toISOString(),
      source: "landing", // útil para tracking
    });

    // opcional: contador total
    await redis.incr("lead:demo:count");

    return res.status(200).json({ ok: true, id });
  } catch (err) {
    console.error(err);
    return res.status(500).json({ error: "Error interno" });
  }
}
