const API        = "/api/fw/db";
const API_SAVE   = "/api/fw/save";
const API_DELETE = "/api/fw/delete";
const API_TEST   = "/api/fw/test";
const API_STATUS = "/api/device/status";

document.getElementById("reload").onclick = loadDevices;

async function loadDevices() {
  const res = await fetch(API);
  const devices = await res.json();

  const tbody = document.querySelector("#devicesTable tbody");
  tbody.innerHTML = "";

  for (const id of Object.keys(devices)) {

    // Obtener FW
    const fw = devices[id];

    // Obtener estado de pago global
    const statusRes = await fetch(`${API_STATUS}?deviceId=${id}`);
    const status = await statusRes.json();

    const paidGlobal = status.paid ? "checked" : "";

    const row = `
      <tr>
        <td>${id}</td>

        <td><input value="${fw.version}" id="v_${id}"></td>
        <td><input value="${fw.url}"     id="u_${id}"></td>

        <td>
          <input type="checkbox" id="pg_${id}" ${paidGlobal}>
          <label>PAGADO</label>
        </td>

        <td><input type="checkbox" id="f_${id}" ${fw.force === "true" ? "checked" : ""}></td>

        <td><input value="${fw.notes}" id="n_${id}"></td>

        <td>
          <button onclick="save('${id}')">💾 Guardar</button>
          <button onclick="removeDevice('${id}')">🗑 Eliminar</button>
          <button onclick="test('${id}')">🧪 Test</button>
        </td>
      </tr>
    `;

    tbody.innerHTML += row;
  }
}

async function save(id) {
  const version = document.getElementById(`v_${id}`).value;
  const url     = document.getElementById(`u_${id}`).value;
  const force   = document.getElementById(`f_${id}`).checked;
  const notes   = document.getElementById(`n_${id}`).value;

  const paidGlobal = document.getElementById(`pg_${id}`).checked;

  // 1) Guardar firmware
  await fetch(API_SAVE, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id, version, url, force, notes })
  });

  // 2) Guardar pagado global
  await fetch("/api/device/pay", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id, paid: paidGlobal })
  });

  alert("✔ Datos guardados correctamente");
}

async function removeDevice(id) {
  if (!confirm(`¿Eliminar config de ${id}?`)) return;

  await fetch(API_DELETE, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id })
  });

  alert("Eliminado");
  loadDevices();
}

async function test(id) {
  const res = await fetch(`${API_TEST}?id=${id}`);
  const data = await res.json();

  alert(`ID: ${id}
Versión: ${data.version}
URL: ${data.url}
Force: ${data.force}`);
}

document.getElementById("newDevice").onclick = async () => {
  const id = prompt("ID del nuevo dispositivo:");
  if (!id) return;

  await fetch(`${API}?device=${encodeURIComponent(id)}`, { method: "POST" });

  loadDevices();
};

loadDevices();
