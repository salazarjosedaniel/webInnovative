const API = "/api/fw/db";

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
  const body = {
    version: document.getElementById("v_" + id).value,
    url: document.getElementById("u_" + id).value,
    force: document.getElementById("f_" + id).checked,
    notes: document.getElementById("n_" + id).value
  };

  await fetch(API + "/" + id, {
    method: "PUT",
    body: JSON.stringify(body),
  });

  alert("Guardado.");
}

function test(id) {
  window.open(`/api/fw/info?deviceId=${id}`);
}

document.getElementById("newDevice").onclick = async () => {
  const id = prompt("ID del nuevo dispositivo:");
  if (!id) return;

  await fetch(API + "/" + id, { method: "POST" });

  loadDevices();
};

loadDevices();