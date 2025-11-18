const API = "/api/fw/db";
const API_SAVE   = "/api/fw/save";
const API_DELETE = "/api/fw/delete";
const API_TEST   = "/api/fw/test";

document.getElementById("reload").onclick = loadDevices;

async function loadDevices() {
  const res = await fetch(API);
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
        <td><input type="checkbox" ${fw.force ? "checked" : ""} id="f_${id}"></td>
        <td><input value="${fw.notes}" id="n_${id}"></td>
        <td>
          <button onclick="save('${id}')">💾 Guardar</button>
          <button onclick="test('${id}')">🧪 Probar</button>
        </td>
      </tr>
    `;
    tbody.innerHTML += row;
  });
}

async function save(id) {
  const version = document.getElementById("v_" + id).value;
  const url     = document.getElementById("u_" + id).value;
  const force   = document.getElementById("f_" + id).checked;
  const notes   = document.getElementById("n_" + id).value;
  const paid    = document.getElementById("p_" + id).checked;

  await fetch(API_SAVE, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id, version, url, paid, force, notes })
  });

  alert("Guardado");
}

async function remove(id) {
  if (!confirm(`¿Eliminar config para ${id}?`)) return;

  await fetch(API_DELETE, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id })
  });

  await loadDevices();
}

async function test(id) {
  const res = await fetch(`${API_TEST}?id=${encodeURIComponent(id)}`);
  const data = await res.json();
  alert(`Firmware para ${id}:\n\nVersion: ${data.version}\nURL: ${data.url}\nForce: ${data.force}`);
}

document.getElementById("newDevice").onclick = async () => {
  const id = prompt("ID del nuevo dispositivo:");
  if (!id) return;

  await fetch(API + "/" + id, { method: "POST" });

  loadDevices();
};

loadDevices();