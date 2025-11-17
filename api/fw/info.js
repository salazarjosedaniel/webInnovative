import { kv } from '@vercel/kv';

export default async function handler(req, res) {
  const { deviceId } = req.query;

  // 1. Leer firmware desde KV
  // Ejemplo de storage:
  // kv.hset("fw:BCV001", { version: "1.0.3", url: "...", force: false, notes: "..." })
  let firmware = await kv.hgetall(`fw:${deviceId}`);

  // 2. Si no existe en KV, cargar firmware default
  if (!firmware || Object.keys(firmware).length === 0) {
    firmware = await kv.hgetall("fw:default");
  }

  // 3. Si tampoco hay default, crear respuesta básica
  if (!firmware) {
    firmware = {
      version: "1.0.0",
      force: false,
      url: "",
      notes: "Sin configuración en KV"
    };
  }

  // 4. Respuesta JSON final
  res.status(200).json({
    deviceId,
    ...firmware,
    timestamp: new Date().toISOString()
  });
}