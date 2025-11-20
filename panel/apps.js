const API = "/api/fw/db";
const API_SAVE = "/api/fw/save";
const API_DELETE = "/api/fw/delete";
const API_TEST = "/api/fw/test";
const API_PAY = "/api/device/pay";

document.getElementById("reload").onclick = loadDevices;
document.getElementById("searchBtn").onclick = searchDevice;

async function loadDevices() {
  const res = await fetch(API);
  const data = await res.json();

  renderTable(data);
  renderDashboard(data);
}

function renderTable(data) {
  const tbody = document.querySelector("#devicesTable tbody");
  tbody.innerHTML = "";

  Object.keys(data).forEach(id => {
    const fw = data[id];

    const row = `
      <tr>
        <td>${id}</td>
        <td><input value="${fw.version}" id="v_${id}"></td>
        <td><input value="${fw.url}" id="u_${id}"></td>
        <td><input type="checkbox" id="pg_${id}" ${fw.paid === "true" ? "checked" : ""}></td>
        <td><input type="checkbox" id="f_${id}" ${fw.force === "true" ? "checked" : ""}></td>
        <td><input value="${fw.notes}" id="n_${id}"></td>
        <td>
          <button onclick="save('${id}')">💾</button>
          <button onclick="removeDevice('${id}')">🗑️</button>
          <button onclick="test('${id}')">🧪</button>
        </td>
      </tr>
    `;

    tbody.innerHTML += row;
  });
}

// ================== BUSCAR ==================
async function searchDevice() {
  const filter = document.getElementById("searchInput").value.toLowerCase();
  const res = await fetch(API);
  const data = await res.json();

  const filtered = {};
  Object.keys(data).forEach(id => {
    if (id.toLowerCase().includes(filter)) {
      filtered[id] = data[id];
    }
  });

  renderTable(filtered);
  renderDashboard(filtered);
}

// ============== GUARDAR ==============
async function save(id) {
  const version = document.getElementById("v_" + id).value;
  const url = document.getElementById("u_" + id).value;
  const force = document.getElementById("f_" + id).checked;
  const notes = document.getElementById("n_" + id).value;
  const paid = document.getElementById("pg_" + id).checked;

  await fetch(API_SAVE, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id, version, url, paid, force, notes })
  });

  // 2) Guardar pagado global 
   await fetch(API_PAY, { 
    method: "POST", 
    headers: { "Content-Type": "application/json" }, 
    body: JSON.stringify({ id, paid: paidGlobal }) 
  });

  alert("Guardado ✓");
}

// ============== ELIMINAR ==============
async function removeDevice(id) {
  if (!confirm(`¿Eliminar ${id}?`)) return;

  await fetch(API_DELETE, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id })
  });

  loadDevices();
}

// ============== PROBAR ==============
async function test(id) {
  const res = await fetch(`${API_TEST}?id=${encodeURIComponent(id)}`);
  const data = await res.json();
  alert(`Firmware para ${id}:\n\nVersión: ${data.version}\nURL: ${data.url}`);
}

// ================= DASHBOARD =================
function renderDashboard(data) {
  const ids = Object.keys(data);
  const total = ids.length;

  let paid = 0;
  let versions = {};

  ids.forEach(id => {
    const fw = data[id];
    if (fw.paid === "true") paid++;
    versions[fw.version] = (versions[fw.version] || 0) + 1;
  });

  document.getElementById("totalDevices").textContent = total;
  document.getElementById("totalPaid").textContent = paid;
  document.getElementById("totalUnpaid").textContent = total - paid;

  drawPie(paid, total - paid);
  drawBar(versions);
}

// ========== GRÁFICA PIE ==========
let pieChart;
function drawPie(paid, unpaid) {
  if (pieChart) pieChart.destroy();

  pieChart = new Chart(document.getElementById("piePaid"), {
    type: "pie",
    data: {
      labels: ["Pagados", "No Pagados"],
      datasets: [{
        data: [paid, unpaid],
        backgroundColor: ["#3cb371", "#e63946"]
      }]
    }
  });
}

// ========== GRÁFICA BARRAS ==========
let barChart;
function drawBar(versions) {
  if (barChart) barChart.destroy();

  barChart = new Chart(document.getElementById("barFirmware"), {
    type: "bar",
    data: {
      labels: Object.keys(versions),
      datasets: [{
        label: "Dispositivos",
        data: Object.values(versions),
        backgroundColor: "#1d3557"
      }]
    }
  });
}

loadDevices();
