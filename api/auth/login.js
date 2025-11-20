import jwt from "jsonwebtoken";

export default function handler(req, res) {
  if (req.method !== "POST")
    return res.status(405).json({ error: "Method not allowed" });

  const { user, pass } = req.body;

  // Puedes cambiar esto por credenciales reales.
  const ADMIN_USER =  "admin";
  const ADMIN_PASS =  "1234";

  if (user !== ADMIN_USER || pass !== ADMIN_PASS) {
    return res.status(200).json({ ok: false });
  }

  // Crear token válido por 24h
  const token = jwt.sign(
    { role: "admin" },
    process.env.JWT_SECRET || "secret-key-super-safe",
    { expiresIn: "24h" }
  );

  res.status(200).json({ ok: true, token });
}
