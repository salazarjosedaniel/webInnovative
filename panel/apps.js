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

// ---------------------
//   CARGAR DEVICES
// ---------------------
async function loadDevices() {
  const res = await fetch(API_DB);
  const data = await res.json();
  const tbody = document.querySelector("#devicesTable tbody");
  tbody.innerHTML = "";
  const deviceID = localStorage.getItem("deviceID");
  Object.keys(data).forEach(id => {
    const fw = data[id];
        if(deviceID == fw.id){
          
              const lastSeenText = fw.lastSeen
              ? new Date(fw.lastSeen).toLocaleString() : "—";

          const statusBadge = isOnline(fw.lastSeen)
              ? `<span class="badge online">🟢 Online</span>`
              : `<span class="badge offline">🔴 Offline</span>`;

          const row = `
            <tr>
              <td>${id}</td>
              <td><input id="v_${id}" value="${fw.version}"></td>
              <td><input id="u_${id}" value="${fw.url}"></td>
              <td><input type="checkbox" id="pg_${id}" ${fw.paid === "true" ? "checked":""}></td>
              <td><input type="checkbox" id="f_${id}" ${fw.force === "true" ? "checked":""}></td>
              <td><input id="n_${id}" value="${fw.notes}"></td>
              <td><input id="e_${id}" value="${fw.name}"></td>
              <td><input id="s_${id}" value="${fw.slogan}"></td>
              <td><input id="i_${id}" value="${fw.instagram}"></td>
              <td><input id="t_${id}" value="${fw.tlf}"></td>
              <td><input id="b_${id}" value="${fw.banco}"></td>
              <td><input id="r_${id}" value="${fw.rif}"></td>
              <td>${lastSeenText}</td>
              <td>${statusBadge}</td>
              <td>
                <button class="btn primary" onclick="save('${id}')">Guardar</button>
                <button class="btn danger" onclick="removeDevice('${id}')">Eliminar</button>
                <button class="btn light" onclick="test('${id}')">Probar</button>
              </td> 
            </tr>
          `;
          tbody.innerHTML += row;
        }
  });
}

document.getElementById("reload").onclick = loadDevices;
document.getElementById("searchBox").oninput = filterDevices;


function filterDevices() {
  const query = document.getElementById("searchBox").value.toLowerCase();

  if (query.trim() === "") {
    return renderTable(cachedDevices);
  }

  const filtered = {};

  Object.keys(cachedDevices).forEach(id => {
    if (id.toLowerCase().includes(query)) {
      filtered[id] = cachedDevices[id];
    }
  });

  renderTable(filtered);
}

function isOnline(lastSeen) {
  if (!lastSeen) return false;
  const diff = Date.now() - new Date(lastSeen).getTime();
  return diff < 180000; // 3 minutos
}


function renderTable(data) {
  const tbody = document.querySelector("#devicesTable tbody");
  tbody.innerHTML = "";
  const deviceID = localStorage.getItem("deviceID");
  Object.keys(data).forEach(id => {
    const fw = data[id];
    if(deviceID == fw.id){
        const lastSeenText = fw.lastSeen
            ? new Date(fw.lastSeen).toLocaleString() : "—";
        const statusBadge = isOnline(fw.lastSeen)
            ? `<span class="badge online">🟢 Online</span>`
            : `<span class="badge offline">🔴 Offline</span>`;

        const row = `
          <tr>
            <td>${id}</td>
            <td><input value="${fw.version}" id="v_${id}"></td>
            <td><input value="${fw.url}" id="u_${id}"></td>
            <td><input type="checkbox" id="pg_${id}" ${fw.paid === "true" ? "checked" : ""}></td>
            <td><input type="checkbox" id="f_${id}" ${fw.force === "true" ? "checked" : ""}></td>
            <td><input value="${fw.notes}" id="n_${id}"></td>
            <td><input value="${fw.name}" id="e_${id}"></td>
            <td><input value="${fw.slogan}" id="s_${id}"></td>
            <td><input value="${fw.instagram}" id="i_${id}"></td>
            <td><input value="${fw.tlf}" id="t_${id}"></td>
            <td><input value="${fw.banco}" id="b_${id}"></td>
            <td><input value="${fw.rif}" id="r_${id}"></td>
            <td>${lastSeenText}</td>
            <td>${statusBadge}</td>
            <td>
              <button onclick="save('${id}')">💾</button>
              <button onclick="removeDevice('${id}')">🗑</button>
              <button onclick="test('${id}')">🧪</button>
            </td>
          </tr>
        `;
        tbody.innerHTML += row;
    }
  });
}

// ---------------------
//   GUARDAR DEVICE
// ---------------------
async function save(id) {
  const body = {
    id,
    version: document.getElementById("v_"+id).value,
    url:     document.getElementById("u_"+id).value,
    paid:    document.getElementById("pg_"+id).checked,
    force:   document.getElementById("f_"+id).checked,
    notes:   document.getElementById("n_"+id).value,
    name:   document.getElementById("e_"+id).value,
    slogan:   document.getElementById("s_"+id).value,
    instagram:   document.getElementById("i_"+id).value,
    tlf:   document.getElementById("t_"+id).value,
    banco:   document.getElementById("b_"+id).value,
    rif:   document.getElementById("r_"+id).value
  };

  const paidGlobal = document.getElementById(`pg_${id}`).checked;
  
  await fetch(API_SAVE, {
    method: "POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify(body)
  });

  // 2) Guardar pagado global
  await fetch(API_PAY, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id, paid: paidGlobal })
  });

  alert("Guardado");
}

// ---------------------
//   ELIMINAR DEVICE
// ---------------------
async function removeDevice(id) {
  if (!confirm("¿Eliminar "+id+"?")) return;

  await fetch(API_DELETE, {
    method: "POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify({id})
  });

  loadDevices();
}

// ---------------------
//   TEST
// ---------------------
async function test(id) {
  const res = await fetch(`${API_TEST}?id=${id}`);
  const d = await res.json();
  alert(JSON.stringify(d,null,2));
}

// ---------------------
//   NUEVO DEVICE
// ---------------------
document.getElementById("newDevice").onclick = async () => {
  const id = prompt("ID del nuevo dispositivo:");
  if (!id) return;

  await fetch(`/api/fw/db?device=${encodeURIComponent(id)}`, { method:"POST" });
  loadDevices();
};

// ---------------------
//   LOGS
// ---------------------
async function loadLogs() {
  const res = await fetch(API_LOGS);
  const logs = await res.text();
  document.getElementById("logsOutput").textContent = logs;
}

// Cargar al inicio
loadDevices();
