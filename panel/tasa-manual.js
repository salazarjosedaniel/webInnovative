const $ = (id) => document.getElementById(id);

const usdInput = $("usd");
const eurInput = $("eur");
const fechaInput = $("fecha");
const resultBox = $("result");

function showResult(data) {
  resultBox.textContent = typeof data === "string" ? data : JSON.stringify(data, null, 2);
}

async function saveManualRate() {
  const usd = parseFloat(usdInput.value);
  const eur = parseFloat(eurInput.value);
  const fecha = fechaInput.value.trim();

  if (Number.isNaN(usd) || Number.isNaN(eur)) {
    showResult("Debes ingresar valores válidos para USD y EUR.");
    return;
  }

  try {
    const res = await fetch("/api/manual", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({
        usd,
        eur,
        fecha
      })
    });

    const data = await res.json();
    showResult(data);
  } catch (err) {
    showResult(`Error guardando tasa manual: ${err.message}`);
  }
}

async function loadManualRate() {
  try {
    const res = await fetch("/api/manual-get");
    const data = await res.json();

    if (data.exists && data.data) {
      usdInput.value = data.data.usd ?? "";
      eurInput.value = data.data.eur ?? "";
      fechaInput.value = data.data.fecha ?? "";
    }

    showResult(data);
  } catch (err) {
    showResult(`Error cargando tasa manual: ${err.message}`);
  }
}

async function clearManualRate() {
  try {
    const res = await fetch("/api/manual-clear", {
      method: "POST"
    });

    const data = await res.json();

    usdInput.value = "";
    eurInput.value = "";
    fechaInput.value = "";

    showResult(data);
  } catch (err) {
    showResult(`Error limpiando tasa manual: ${err.message}`);
  }
}

$("btnSave").addEventListener("click", saveManualRate);
$("btnLoad").addEventListener("click", loadManualRate);
$("btnClear").addEventListener("click", clearManualRate);