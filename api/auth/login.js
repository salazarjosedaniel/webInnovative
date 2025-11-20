import { serialize } from "cookie";

export default async function handler(req, res) {
  if (req.method !== "POST")
    return res.status(405).json({ error: "Method not allowed" });

  const { user, pass } = JSON.parse(req.body || "{}");

  if (!user || !pass)
    return res.status(400).json({ error: "Missing fields" });

  // Validar credenciales desde env
  if (user !== process.env.ADMIN_USER || pass !== process.env.ADMIN_PASS)
    return res.status(401).json({ error: "Invalid credentials" });

  // Crear token simple
  const token = "SESSION-" + Date.now();

  // Guardar cookie (1 día)
  res.setHeader(
    "Set-Cookie",
    serialize("session_token", token, {
      path: "/",
      httpOnly: true,
      sameSite: "strict",
      maxAge: 60 * 60 * 24,
    })
  );

  return res.status(200).json({ ok: true });
}
