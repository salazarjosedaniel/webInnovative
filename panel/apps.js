// =======================
//  ENDPOINTS DEL BACKEND
// =======================
const API_DEVICES = "/api/fw/db";
const API_SAVE   = "/api/fw/save";
const API_DELETE = "/api/fw/delete";
const API_TEST   = "/api/fw/test";
const API_LOGS   = "/api/logs/list";
const API_CLEAR  = "/api/logs/clear";


// =======================
//        TABS
// =======================
document.querySelectorAll(".tab").forEach(t => {
  t.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach(x => x.classList.remove("active"));
    document.querySelectorAll(".content").forEach(c => c.classList.remove("active"));

    t.classList.add("active");
    document.getElementById(t.dataset.tab).classList.add("active");

    if (t.dataset.tab === "logs") loadLogs();
  });
});


// =======================
//      CARGAR DISPOSITIVOS
// =======================
document.getElementById("reload").onclick = loadDevices;

async function loadDevices() {
  const res = await fetch(API_DEVICES);
  const data = await res.json();

  const tbody = document.querySelector("#devicesTable tbody");
  tbody.innerHTML = "";

  Object.keys(data).forEach(id => {
    const fw = data[id];

    const row = `
      <tr>
        <td>${id}</td>
        <td><input value="${fw.version}" id="v_${id}"></td>
        <td><input value="${fw.url}" id="u_${id}"></td>
        <td><input type="checkbox" id="p_${id}" ${fw.paid === "true" ? "checked" : ""}></td>
        <td><input type="checkbox" id="f_${id}" ${fw.force === "true" ? "checked" : ""}></td>
        <td><input value="${fw.notes}" id="n_${id}"></td>

        <td>
          <button onclick="save('${id}')">💾</button>
          <button class="delete" onclick="removeDevice('${id}')">🗑️</button>
          <button class="test" onclick="testDevice('${id}')">🧪</button>
        </td>
      </tr>
    `;
    tbody.innerHTML += row;
  });
}


// =======================
//        GUARDAR
// =======================
async function save(id) {
  const payload = {
    id,
    version: document.getElementById("v_" + id).value,
    url:     document.getElementById("u_" + id).value,
    paid:    document.getElementById("p_" + id).checked,
    force:   document.getElementById("f_" + id).checked,
    notes:   document.getElementById("n_" + id).value
  };

  await fetch(API_SAVE, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  });

  alert("Guardado correctamente");
  loadDevices();
}


// =======================
//        ELIMINAR
// =======================
async function removeDevice(id) {
  if (!confirm(`¿Eliminar dispositivo ${id}?`)) return;

  await fetch(API_DELETE, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id })
  });

  loadDevices();
}


// =======================
//         TEST
// =======================
async function testDevice(id) {
  const res = await fetch(`${API_TEST}?id=${encodeURIComponent(id)}`);
  const data = await res.json();

  alert(`Test Firmware\n\nVersión: ${data.version}\nURL: ${data.url}`);
}


// =======================
//        NUEVO DEVICE
// =======================
document.getElementById("newDevice").onclick = async () => {
  const id = prompt("Ingrese nuevo Device ID:");
  if (!id) return;

  await fetch(`${API_DEVICES}?device=${encodeURIComponent(id)}`, {
    method: "POST"
  });

  loadDevices();
};


// =======================
//         LOGS
// =======================
async function loadLogs() {
  const res = await fetch(API_LOGS);
  const logs = await res.json();

  const box = document.getElementById("logsBox");
  box.innerHTML = logs.join("\n");
  box.scrollTop = box.scrollHeight;
}

document.getElementById("clearLogs").onclick = async () => {
  await fetch(API_CLEAR, { method: "POST" });
  loadLogs();
};
