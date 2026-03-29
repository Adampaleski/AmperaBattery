#include <Arduino.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "dashboard_config.h"

HardwareSerial teensyLink(2);
WebServer server(80);

String gTelemetryLine;
String gLatestTelemetry = "{\"status_text\":\"Waiting for Teensy telemetry\",\"modules\":0,\"pack_voltage_v\":0,\"soc_percent\":0}";
String gWifiMode = "booting";
unsigned long gLastTelemetryMs = 0;

const char kDashboardHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Amp BMS Dashboard</title>
  <style>
    :root {
      --bg: #f4efe5;
      --panel: rgba(255, 250, 242, 0.88);
      --ink: #1f2a24;
      --muted: #59655d;
      --accent: #0f7a5c;
      --accent-2: #db8b2b;
      --line: rgba(31, 42, 36, 0.12);
      --shadow: 0 18px 40px rgba(46, 52, 48, 0.12);
    }

    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Avenir Next", "Segoe UI", sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at top left, rgba(219, 139, 43, 0.22), transparent 32%),
        radial-gradient(circle at top right, rgba(15, 122, 92, 0.20), transparent 35%),
        linear-gradient(180deg, #f9f4eb 0%, var(--bg) 100%);
    }

    main {
      max-width: 1100px;
      margin: 0 auto;
      padding: 24px 18px 40px;
    }

    .hero,
    .panel,
    .card {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 22px;
      box-shadow: var(--shadow);
      backdrop-filter: blur(10px);
    }

    .hero {
      padding: 24px;
      display: grid;
      gap: 18px;
    }

    .eyebrow {
      margin: 0;
      font-size: 0.78rem;
      letter-spacing: 0.16em;
      text-transform: uppercase;
      color: var(--muted);
    }

    h1 {
      margin: 4px 0 0;
      font-size: clamp(2rem, 6vw, 4rem);
      line-height: 0.96;
      letter-spacing: -0.04em;
    }

    .hero-grid {
      display: grid;
      gap: 12px;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    }

    .hero-stat {
      padding: 14px 16px;
      border-radius: 18px;
      background: rgba(255, 255, 255, 0.58);
      border: 1px solid rgba(31, 42, 36, 0.09);
    }

    .hero-stat small,
    .card small,
    .meta span {
      display: block;
      color: var(--muted);
      font-size: 0.8rem;
      letter-spacing: 0.04em;
      text-transform: uppercase;
    }

    .hero-stat strong {
      display: block;
      margin-top: 8px;
      font-size: 1.6rem;
      letter-spacing: -0.03em;
    }

    .meta {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      align-items: center;
    }

    .pill {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 10px 14px;
      border-radius: 999px;
      background: rgba(15, 122, 92, 0.10);
      border: 1px solid rgba(15, 122, 92, 0.18);
      font-weight: 700;
    }

    .pill::before {
      content: "";
      width: 10px;
      height: 10px;
      border-radius: 999px;
      background: var(--accent);
      box-shadow: 0 0 0 4px rgba(15, 122, 92, 0.15);
    }

    .pill.stale {
      background: rgba(219, 139, 43, 0.13);
      border-color: rgba(219, 139, 43, 0.28);
    }

    .pill.stale::before {
      background: var(--accent-2);
      box-shadow: 0 0 0 4px rgba(219, 139, 43, 0.16);
    }

    .panel {
      margin-top: 18px;
      padding: 18px;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 14px;
    }

    .card {
      padding: 16px;
    }

    .value {
      margin-top: 10px;
      font-size: 2rem;
      line-height: 0.96;
      letter-spacing: -0.05em;
    }

    .sub {
      margin-top: 8px;
      color: var(--muted);
      font-size: 0.92rem;
    }

    pre {
      margin: 0;
      padding: 14px;
      overflow: auto;
      border-radius: 16px;
      background: #17201c;
      color: #f3efe8;
      font-size: 0.88rem;
    }

    a {
      color: var(--accent);
      font-weight: 700;
      text-decoration: none;
    }

    @media (max-width: 640px) {
      main {
        padding-inline: 14px;
      }

      .hero,
      .panel,
      .card {
        border-radius: 18px;
      }

      .value {
        font-size: 1.7rem;
      }
    }
  </style>
</head>
<body>
  <main>
    <section class="hero">
      <div>
        <p class="eyebrow">Volt / Ampera Pack</p>
        <h1>Battery Dashboard</h1>
      </div>
      <div class="meta">
        <div id="statusPill" class="pill">Waiting for telemetry</div>
        <span id="networkText">Network unknown</span>
        <span id="ageText">No live data yet</span>
        <a href="/update">ESP32 OTA</a>
      </div>
      <div class="hero-grid">
        <div class="hero-stat">
          <small>Pack Voltage</small>
          <strong id="packVoltage">0.0 V</strong>
        </div>
        <div class="hero-stat">
          <small>Pack Current</small>
          <strong id="packCurrent">0.0 A</strong>
        </div>
        <div class="hero-stat">
          <small>State Of Charge</small>
          <strong id="socPercent">0 %</strong>
        </div>
        <div class="hero-stat">
          <small>Modules / Cells</small>
          <strong id="moduleCount">0 / 0</strong>
        </div>
      </div>
    </section>

    <section class="panel">
      <div class="grid">
        <article class="card">
          <small>Low Cell</small>
          <div id="lowCell" class="value">0.000 V</div>
          <div class="sub">Weakest cell seen in the active string.</div>
        </article>
        <article class="card">
          <small>High Cell</small>
          <div id="highCell" class="value">0.000 V</div>
          <div class="sub">Highest cell voltage seen in the active string.</div>
        </article>
        <article class="card">
          <small>Cell Delta</small>
          <div id="cellDelta" class="value">0 mV</div>
          <div class="sub">Spread between the lowest and highest cells.</div>
        </article>
        <article class="card">
          <small>Average Temp</small>
          <div id="avgTemp" class="value">0.0 C</div>
          <div id="tempRange" class="sub">Low 0.0 C / High 0.0 C</div>
        </article>
        <article class="card">
          <small>Charge Limit</small>
          <div id="chargeLimit" class="value">0.0 A</div>
          <div class="sub">Current limit the Teensy BMS is advertising.</div>
        </article>
        <article class="card">
          <small>Discharge Limit</small>
          <div id="dischargeLimit" class="value">0.0 A</div>
          <div class="sub">Current limit the Teensy BMS is advertising.</div>
        </article>
        <article class="card">
          <small>BMS State</small>
          <div id="bmsState" class="value">Unknown</div>
          <div id="errorReason" class="sub">Error reason: 0</div>
        </article>
        <article class="card">
          <small>Contactor Bits</small>
          <div id="contactorBits" class="value">0</div>
          <div id="inputState" class="sub">IN1 0 IN2 0 IN3 0 IN4 0</div>
        </article>
      </div>
    </section>

    <section class="panel">
      <small>Latest Telemetry JSON</small>
      <pre id="rawJson">{}</pre>
    </section>
  </main>

  <script>
    const ids = {
      packVoltage: document.getElementById('packVoltage'),
      packCurrent: document.getElementById('packCurrent'),
      socPercent: document.getElementById('socPercent'),
      moduleCount: document.getElementById('moduleCount'),
      lowCell: document.getElementById('lowCell'),
      highCell: document.getElementById('highCell'),
      cellDelta: document.getElementById('cellDelta'),
      avgTemp: document.getElementById('avgTemp'),
      tempRange: document.getElementById('tempRange'),
      chargeLimit: document.getElementById('chargeLimit'),
      dischargeLimit: document.getElementById('dischargeLimit'),
      bmsState: document.getElementById('bmsState'),
      errorReason: document.getElementById('errorReason'),
      contactorBits: document.getElementById('contactorBits'),
      inputState: document.getElementById('inputState'),
      rawJson: document.getElementById('rawJson'),
      statusPill: document.getElementById('statusPill'),
      networkText: document.getElementById('networkText'),
      ageText: document.getElementById('ageText')
    };

    const format = (value, digits = 1) => Number(value || 0).toFixed(digits);

    async function refreshDashboard() {
      try {
        const [telemetryRes, systemRes] = await Promise.all([
          fetch('/api/telemetry', { cache: 'no-store' }),
          fetch('/api/system', { cache: 'no-store' })
        ]);

        const telemetry = await telemetryRes.json();
        const system = await systemRes.json();
        const stale = Number(system.telemetry_age_ms || 0) > 4000;

        ids.packVoltage.textContent = `${format(telemetry.pack_voltage_v, 2)} V`;
        ids.packCurrent.textContent = `${format(telemetry.current_a, 2)} A`;
        ids.socPercent.textContent = `${Math.round(Number(telemetry.soc_percent || 0))} %`;
        ids.moduleCount.textContent = `${Math.round(Number(telemetry.modules || 0))} / ${Math.round(Number(telemetry.series_cells || 0))}`;
        ids.lowCell.textContent = `${format(telemetry.low_cell_v, 3)} V`;
        ids.highCell.textContent = `${format(telemetry.high_cell_v, 3)} V`;
        ids.cellDelta.textContent = `${Math.round(Number(telemetry.cell_delta_mv || 0))} mV`;
        ids.avgTemp.textContent = `${format(telemetry.avg_temp_c, 1)} C`;
        ids.tempRange.textContent = `Low ${format(telemetry.low_temp_c, 1)} C / High ${format(telemetry.high_temp_c, 1)} C`;
        ids.chargeLimit.textContent = `${format(telemetry.charge_limit_a, 1)} A`;
        ids.dischargeLimit.textContent = `${format(telemetry.discharge_limit_a, 1)} A`;
        ids.bmsState.textContent = telemetry.status_text || 'Unknown';
        ids.errorReason.textContent = `Error reason: ${telemetry.error_reason || 0}`;
        ids.contactorBits.textContent = `${telemetry.contactor_bits || 0}`;
        ids.inputState.textContent = `IN1 ${telemetry.input_1 || 0} IN2 ${telemetry.input_2 || 0} IN3 ${telemetry.input_3 || 0} IN4 ${telemetry.input_4 || 0}`;
        ids.rawJson.textContent = JSON.stringify(telemetry, null, 2);

        ids.statusPill.textContent = stale ? 'Telemetry stale' : (telemetry.status_text || 'Live');
        ids.statusPill.classList.toggle('stale', stale);
        ids.networkText.textContent = `${String(system.wifi_mode || 'unknown').toUpperCase()} ${system.ip || '0.0.0.0'} ${system.hostname || ''}`.trim();
        ids.ageText.textContent = `Telemetry age ${Math.round(Number(system.telemetry_age_ms || 0))} ms`;
      } catch (error) {
        ids.statusPill.textContent = 'Dashboard offline';
        ids.statusPill.classList.add('stale');
        ids.ageText.textContent = error.message;
      }
    }

    refreshDashboard();
    setInterval(refreshDashboard, 1000);
  </script>
</body>
</html>
)HTML";

const char kUpdateHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 OTA</title>
  <style>
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      font-family: "Avenir Next", "Segoe UI", sans-serif;
      background: linear-gradient(180deg, #efe8dc 0%, #f7f2eb 100%);
      color: #1f2a24;
    }

    main {
      width: min(540px, calc(100vw - 28px));
      padding: 28px;
      border-radius: 24px;
      background: rgba(255, 251, 245, 0.92);
      border: 1px solid rgba(31, 42, 36, 0.12);
      box-shadow: 0 20px 42px rgba(46, 52, 48, 0.14);
    }

    h1 {
      margin-top: 0;
      font-size: clamp(1.8rem, 6vw, 3rem);
      line-height: 0.98;
    }

    p {
      color: #59655d;
      line-height: 1.5;
    }

    input,
    button {
      width: 100%;
      padding: 14px 16px;
      border-radius: 14px;
      border: 1px solid rgba(31, 42, 36, 0.15);
      font: inherit;
      margin-top: 12px;
    }

    button {
      cursor: pointer;
      font-weight: 700;
      color: #fff7ef;
      background: #0f7a5c;
      border: none;
    }

    a {
      display: inline-block;
      margin-top: 16px;
      color: #0f7a5c;
      font-weight: 700;
      text-decoration: none;
    }
  </style>
</head>
<body>
  <main>
    <h1>ESP32 OTA Upload</h1>
    <p>Upload the compiled ESP32 <code>.bin</code> file from PlatformIO. This updates the WiFi dashboard module only, not the Teensy BMS firmware.</p>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="update" accept=".bin" required>
      <button type="submit">Install Update</button>
    </form>
    <a href="/">Back to dashboard</a>
  </main>
</body>
</html>
)HTML";

bool wifiCredentialsConfigured() {
  return strlen(kWifiSsid) > 0;
}

String currentIpAddress() {
  if (gWifiMode == "sta") {
    return WiFi.localIP().toString();
  }
  return WiFi.softAPIP().toString();
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kFallbackApSsid, kFallbackApPassword);
  gWifiMode = "ap";
}

void connectWifi() {
  WiFi.setSleep(false);

  if (!wifiCredentialsConfigured()) {
    startAccessPoint();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kDashboardHostname);
  WiFi.begin(kWifiSsid, kWifiPassword);

  const unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < kWifiConnectTimeoutMs) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    gWifiMode = "sta";
    return;
  }

  WiFi.disconnect(true, true);
  delay(250);
  startAccessPoint();
}

void readTeensyTelemetry() {
  while (teensyLink.available() > 0) {
    const char incoming = static_cast<char>(teensyLink.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      gTelemetryLine.trim();
      if (gTelemetryLine.length() > 1 && gTelemetryLine.startsWith("{") && gTelemetryLine.endsWith("}")) {
        gLatestTelemetry = gTelemetryLine;
        gLastTelemetryMs = millis();
      }
      gTelemetryLine = "";
      continue;
    }

    if (gTelemetryLine.length() < 768) {
      gTelemetryLine += incoming;
    } else {
      gTelemetryLine = "";
    }
  }
}

void handleDashboard() {
  server.send_P(200, "text/html", kDashboardHtml);
}

void handleTelemetry() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", gLatestTelemetry);
}

void handleSystemInfo() {
  String response;
  response.reserve(256);
  response += "{\"wifi_mode\":\"";
  response += gWifiMode;
  response += "\",\"ip\":\"";
  response += currentIpAddress();
  response += "\",\"hostname\":\"";
  response += kDashboardHostname;
  response += "\",\"telemetry_age_ms\":";
  response += String(gLastTelemetryMs == 0 ? 0 : millis() - gLastTelemetryMs);
  response += ",\"esp32_uptime_ms\":";
  response += String(millis());
  response += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", response);
}

void handleUpdatePage() {
  server.send_P(200, "text/html", kUpdateHtml);
}

void handleUpdateResult() {
  const bool success = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(success ? 200 : 500, "text/plain", success ? "ESP32 update complete. Rebooting." : "ESP32 update failed.");
  delay(250);
  if (success) {
    ESP.restart();
  }
}

void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) {
      Update.printError(Serial);
    }
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleDashboard);
  server.on("/api/telemetry", HTTP_GET, handleTelemetry);
  server.on("/api/system", HTTP_GET, handleSystemInfo);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateResult, handleUpdateUpload);
  server.on("/favicon.ico", HTTP_GET, []() {
    server.send(204);
  });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  gTelemetryLine.reserve(768);
  gLatestTelemetry.reserve(768);

  teensyLink.begin(kTeensyLinkBaud, SERIAL_8N1, kTeensyLinkRxPin, kTeensyLinkTxPin);

  connectWifi();

  if (MDNS.begin(kDashboardHostname)) {
    MDNS.addService("http", "tcp", 80);
  }

  setupWebServer();
}

void loop() {
  readTeensyTelemetry();
  server.handleClient();
}
