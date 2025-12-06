// Rutas API
const API_DB     = "/api/fw/db";
const API_SAVE   = "/api/fw/save";
const API_DELETE = "/api/fw/delete";
const API_TEST   = "/api/fw/test";
const API_LOGS   = "/api/device/logs";
const API_PAY    = "/api/device/pay";

// ---------------------
//   CAMBIAR SECCIONES
// ---------------------
document.querySelectorAll(".tab-button").forEach(btn => {
  btn.onclick = () => {
    document.querySelectorAll(".tab-button").forEach(b => b.classList.remove("active"));
    document.querySelectorAll(".tab-content").forEach(c => c.classList.remove("active"));

    btn.classList.add("active");
    document.getElementById(btn.dataset.tab).classList.add("active");

    if (btn.dataset.tab === "logs") loadLogs();
  };
});

// ======================================================
//   CARGAR DEVICES — AHORA GENERA TARJETAS (no tabla)
// ======================================================
let cachedDevices = {};

async function loadDevices() {
  const res = await fetch(API_DB);
  const data = await res.json();
  cachedDevices = data;

  renderCards(data);
}

// ======================================================
//   NUEVO RENDER PREMIUM CON CARDS
// ======================================================

function renderCards(data) {
  const container = document.getElementById("devicesContainer");
  container.innerHTML = "";

  if (!data || Object.keys(data).length === 0) {
    container.innerHTML = "<p>No hay dispositivos registrados.</p>";
    return;
  }

  Object.keys(data).forEach(id => {
    const fw = data[id];

    const lastSeenText = fw.lastSeen
      ? new Date(fw.lastSeen).toLocaleString()
      : "—";

    const isOnline = fw.lastSeen && (Date.now() - new Date(fw.lastSeen).getTime() < 180000);

    const card = document.createElement("div");
    card.className = "device-card";

    card.innerHTML = `
      <div class="device-title">${id}</div>

      <div class="device-field"><span class="device-label">Versión:</span> ${fw.version || "-"}</div>
      <div class="device-field"><span class="device-label">Firmware:</span> ${fw.url || "-"}</div>
      <div class="device-field"><span class="device-label">Notas:</span> ${fw.notes || "-"}</div>
      <div class="device-field"><span class="device-label">Última conexión:</span> ${lastSeenText}</div>

      <div class="device-badges">
        <span class="badge ${fw.paid === "true" ? "badge-paid" : "badge-unpaid"}">
          Pagado: ${fw.paid === "true" ? "Sí" : "No"}
        </span>

        <span class="badge ${fw.force === "true" ? "badge-force" : "badge-unpaid"}">
          Force: ${fw.force === "true" ? "Sí" : "No"}
        </span>

        <span class="badge ${isOnline ? "online" : "offline"}">
          ${isOnline ? "Online" : "Offline"}
        </span>
      </div>

      <div class="device-actions">
        <button class="btn primary" onclick='openModal(${JSON.stringify({ id, ...fw })})'>Editar</button>
        <button class="btn primary" onclick="toggleForceCard('${id}')">Force</button>
        <button class="btn success" onclick="togglePaidCard('${id}')">Pagado</button>
        <button class="btn light" onclick="test('${id}')">Probar</button>
        <button class="btn danger" onclick="removeDevice('${id}')">Eliminar</button>
      </div>
    `;

    container.appendChild(card);
  });
}


// ======================================================
//   TOGGLE desde CARD (usa tu misma API SAVE / PAY)
// ======================================================
async function toggleForceCard(id) {
  const newForce = !(cachedDevices[id].force === "true");

  await fetch(API_SAVE, {
    method: "POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify({ id, force: newForce })
  });

  cachedDevices[id].force = newForce.toString();
  renderCards(cachedDevices);
}

async function togglePaidCard(id) {
  const newPaid = !(cachedDevices[id].paid === "true");

  await fetch(API_PAY, {
    method: "POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify({ id, paid: newPaid })
  });

  cachedDevices[id].paid = newPaid.toString();
  renderCards(cachedDevices);
}

// ======================================================
//   BÚSQUEDA FILTRADA
// ======================================================
document.getElementById("searchBox").oninput = filterDevices;

function filterDevices() {
  const query = document.getElementById("searchBox").value.toLowerCase();

  if (query.trim() === "") {
    return renderCards(cachedDevices);
  }

  const filtered = {};

  Object.keys(cachedDevices).forEach(id => {
    if (id.toLowerCase().includes(query)) {
      filtered[id] = cachedDevices[id];
    }
  });

  renderCards(filtered);
}

function isOnline(lastSeen) {
  if (!lastSeen) return false;
  const diff = Date.now() - new Date(lastSeen).getTime();
  return diff < 180000; // 3 minutos
}

// ======================================================
//   GUARDAR (igual que estaba, sin cambios)
// ======================================================
async function save(id) {
  const body = {
    id,
    version: document.getElementById("v_"+id)?.value,
    url:     document.getElementById("u_"+id)?.value,
    paid:    document.getElementById("pg_"+id)?.checked,
    force:   document.getElementById("f_"+id)?.checked,
    notes:   document.getElementById("n_"+id)?.value,
    name:    document.getElementById("e_"+id)?.value,
    slogan:  document.getElementById("s_"+id)?.value,
    instagram: document.getElementById("i_"+id)?.value,
    tlf: document.getElementById("t_"+id)?.value,
    banco: document.getElementById("b_"+id)?.value,
    rif: document.getElementById("r_"+id)?.value
  };

  const paidGlobal = document.getElementById(`pg_${id}`)?.checked;

  await fetch(API_SAVE, {
    method: "POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify(body)
  });

  await fetch(API_PAY, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id, paid: paidGlobal })
  });

  alert("Guardado");
}

// ======================================================
//   ELIMINAR
// ======================================================
async function removeDevice(id) {
  if (!confirm("¿Eliminar "+id+"?")) return;

  await fetch(API_DELETE, {
    method: "POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify({id})
  });

  loadDevices();
}

// ======================================================
//   TEST
// ======================================================
async function test(id) {
  const res = await fetch(`${API_TEST}?id=${id}`);
  const d = await res.json();
  alert(JSON.stringify(d,null,2));
}

// ======================================================
//   NUEVO DEVICE (igual que estaba)
// ======================================================
document.getElementById("newDevice").onclick = async () => {
  const id = prompt("ID del nuevo dispositivo:");
  if (!id) return;

  await fetch(`/api/fw/db?device=${encodeURIComponent(id)}`, { method:"POST" });
  loadDevices();
};

// ======================================================
//   LOGS
// ======================================================
async function loadLogs() {
  const res = await fetch(API_LOGS);
  const logs = await res.text();
  document.getElementById("logsOutput").textContent = logs;
}

// Cargar al inicio
loadDevices();
