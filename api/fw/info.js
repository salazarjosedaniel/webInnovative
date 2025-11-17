import { kv } from "@vercel/kv";

export default async function handler(req, res) {
  const { deviceId } = req.query;

  try {
    // === 1. Intentar leer firmware personalizado desde KV ===
    // Clave: fw:BCV001, fw:BCV002...
    let firmware = await kv.hgetall(`fw:${deviceId}`);

    // === 2. Si no existe un firmware para ese deviceId ===
    if (!firmware || Object.keys(firmware).length === 0) {
      firmware = await kv.hgetall("fw:default");
    }

    // === 3. Si tampoco hay default, crear uno básico ===
    if (!firmware) {
      firmware = {
        version: "1.0.0",
        force: false,
        url: "",
        notes: "Sin configuración de firmware"
      };
    }

    // === 4. Responder ===
    res.status(200).json({
      deviceId: deviceId || "unknown",
      ...firmware,
      timestamp: new Date().toISOString()
    });

  } catch (err) {
    console.error("❌ Error leyendo KV:", err);
    res.status(500).json({ error: "Error de servidor leyendo firmware" + err });
  }
}
