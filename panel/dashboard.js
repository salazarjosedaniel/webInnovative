// panel/dashboard.js

const API_SUMMARY = "/api/dashboard/summary";

// Opcional: si ya tienes login, proteger la vista
(function checkAuth() {
  const token = localStorage.getItem("bcv_token");
  if (!token) {
    // si aún no tienes tokens, puedes comentar esto
    // window.location.href = "/login.html";
  }
})();

document.getElementById("logoutBtn")?.addEventListener("click", () => {
  localStorage.removeItem("bcv_token");
  window.location.href = "/login.html";
});

let chartPaid, chartStatus, chartFirmware;

async function loadDashboard() {
  try {
    const res = await fetch(API_SUMMARY);
    if (!res.ok) throw new Error("HTTP " + res.status);
    const data = await res.json();

    // KPI
    const total = data.totalDevices || 0;
    const paidTrue = data.paid?.true || 0;
    const paidFalse = data.paid?.false || 0;
    const online = data.status?.online || 0;
    const offline = data.status?.offline || 0;

    document.getElementById("kpiTotal").textContent = total;
    document.getElementById("kpiPaid").textContent = paidTrue;
    document.getElementById("kpiUnpaid").textContent = paidFalse;
    document.getElementById("kpiOnline").textContent = online;
    document.getElementById("kpiOffline").textContent = offline;

    document.getElementById("dashboardMeta").textContent =
      `Datos generados: ${new Date(data.generatedAt).toLocaleString()}`;

    // 1) Gráfico de pago (doughnut)
    const paidCtx = document.getElementById("chartPaid").getContext("2d");
    if (chartPaid) chartPaid.destroy();
    chartPaid = new Chart(paidCtx, {
      type: "doughnut",
      data: {
        labels: ["Pagado", "No pagado"],
        datasets: [{
          data: [paidTrue, paidFalse],
          // colores por defecto de Chart.js
        }]
      },
      options: {
        plugins: {
          legend: { position: "bottom" }
        }
      }
    });

    // 2) Gráfico de estado online/offline (doughnut)
    const statusCtx = document.getElementById("chartStatus").getContext("2d");
    if (chartStatus) chartStatus.destroy();
    chartStatus = new Chart(statusCtx, {
      type: "doughnut",
      data: {
        labels: ["Online", "Offline"],
        datasets: [{
          data: [online, offline]
        }]
      },
      options: {
        plugins: {
          legend: { position: "bottom" }
        }
      }
    });

    // 3) Gráfico de distribución de firmware (bar)
    const fwLabels = Object.keys(data.firmwareUsage || {});
    const fwValues = fwLabels.map(v => data.firmwareUsage[v]);

    const fwCtx = document.getElementById("chartFirmware").getContext("2d");
    if (chartFirmware) chartFirmware.destroy();
    chartFirmware = new Chart(fwCtx, {
      type: "bar",
      data: {
        labels: fwLabels,
        datasets: [{
          label: "Dispositivos por versión",
          data: fwValues
        }]
      },
      options: {
        plugins: {
          legend: { display: false }
        },
        scales: {
          x: { title: { display: true, text: "Versión firmware" } },
          y: { title: { display: true, text: "Dispositivos" }, beginAtZero: true }
        }
      }
    });

  } catch (err) {
    console.error("Error cargando dashboard:", err);
    document.getElementById("dashboardMeta").textContent =
      "Error al cargar datos del dashboard.";
  }
}

document.addEventListener("DOMContentLoaded", loadDashboard);
