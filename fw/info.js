export default function handler(req, res) {
  const { deviceId } = req.query;

  // ======== 1. Lista de firmware por dispositivo (personalizable) ========
  const firmwareDB = {
    "BCV001": {
      version: "1.0.3",
      force: false,
      url: "https://web-innovative.vercel.app/fw/firmware-1.0.3.bin",
      notes: "Mejoras en reconexión WiFi y actualización de 7 segmentos."
    },
    "BCV002": {
      version: "1.0.3",
      force: false,
      url: "https://web-innovative.vercel.app/fw/firmware-1.0.3.bin",
      notes: "Versión estándar"
    }
  };

  // ======== 2. Versión por defecto para dispositivos no registrados ========
  const defaultFirmware = {
    version: "1.0.3",
    force: false,
    url: "https://web-innovative.vercel.app/fw/firmware-1.0.3.bin",
    notes: "Versión general para dispositivos BCV Display."
  };

  // ======== 3. Selección del firmware =============
  const firmware = firmwareDB[deviceId] || defaultFirmware;

  // ======== 4. Respuesta JSON ============
  res.status(200).json({
    deviceId: deviceId || "unknown",
    ...firmware,
    timestamp: new Date().toISOString()
  });
}
