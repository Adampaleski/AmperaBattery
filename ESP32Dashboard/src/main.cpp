#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <mcp2515.h>

#include "dashboard_config.h"

HardwareSerial teensyLink(2);
WebServer server(80);
Preferences prefs;
MCP2515 chargerCan(kMcp2515CsPin);

struct RuntimeConfig {
  String wifiSsid;
  String wifiPassword;
  bool chargerEnabled = true;
  uint32_t chargerCanId = kElconCanId;
  uint32_t chargerCanBaudKbps = kElconCanBaudKbps;
  bool chargerCanOsc8MHz = kElconCanOscillator8MHz;
  uint32_t chargerCommandPeriodMs = 100;
  uint32_t telemetryStaleMs = 2500;
};

struct ParsedTelemetry {
  bool valid = false;
  float chargeTargetV = 0.0f;
  float chargeLimitA = 0.0f;
  float packVoltageV = 0.0f;
  int chargeState = 0;
  int bmsStatusCode = 0;
  String bmsStatusText = "Unknown";
};

struct ChargerRuntime {
  bool canReady = false;
  bool commandActive = false;
  bool staleTelemetry = true;
  uint32_t txCount = 0;
  uint32_t rxCount = 0;
  uint32_t errCount = 0;
  uint32_t lastRxId = 0;
  String lastRxDataHex = "";
  float lastCommandV = 0.0f;
  float lastCommandA = 0.0f;
  unsigned long lastCommandMs = 0;
  unsigned long lastRxMs = 0;
};

RuntimeConfig gConfig;
ParsedTelemetry gTelemetry;
ChargerRuntime gCharger;

String gTelemetryLine;
String gLatestTelemetry = "{\"status_text\":\"Waiting for Teensy telemetry\",\"modules\":0,\"pack_voltage_v\":0,\"soc_percent\":0}";
String gWifiMode = "booting";
unsigned long gLastTelemetryMs = 0;
unsigned long gLastChargerTickMs = 0;

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
        <span id="chargerText">Charger bridge unknown</span>
        <a href="/update">ESP32 OTA</a>
        <a href="/config">Config</a>
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
          <div class="sub">Current limit from Teensy BMS.</div>
        </article>
        <article class="card">
          <small>Charge Target</small>
          <div id="chargeTarget" class="value">0.0 V</div>
          <div class="sub">Pack charge target from Teensy settings.</div>
        </article>
        <article class="card">
          <small>BMS State</small>
          <div id="bmsState" class="value">Unknown</div>
          <div id="errorReason" class="sub">Error reason: 0</div>
        </article>
        <article class="card">
          <small>Charger CAN</small>
          <div id="chargerState" class="value">Idle</div>
          <div id="chargerStats" class="sub">TX 0 RX 0 ERR 0</div>
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
      chargeTarget: document.getElementById('chargeTarget'),
      bmsState: document.getElementById('bmsState'),
      errorReason: document.getElementById('errorReason'),
      chargerState: document.getElementById('chargerState'),
      chargerStats: document.getElementById('chargerStats'),
      rawJson: document.getElementById('rawJson'),
      statusPill: document.getElementById('statusPill'),
      networkText: document.getElementById('networkText'),
      ageText: document.getElementById('ageText'),
      chargerText: document.getElementById('chargerText')
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
        const stale = Number(system.telemetry_age_ms || 0) > Number(system.telemetry_stale_ms || 3000);

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
        ids.chargeTarget.textContent = `${format(telemetry.charge_target_v, 2)} V`;
        ids.bmsState.textContent = telemetry.status_text || 'Unknown';
        ids.errorReason.textContent = `Error reason: ${telemetry.error_reason || 0}`;
        ids.rawJson.textContent = JSON.stringify(telemetry, null, 2);

        ids.chargerState.textContent = system.charger_command_active ? 'Commanding' : 'Standby';
        ids.chargerStats.textContent = `TX ${system.charger_tx_count || 0} RX ${system.charger_rx_count || 0} ERR ${system.charger_err_count || 0}`;
        ids.chargerText.textContent = `Elcon CAN ${system.charger_can_kbps || 0}k ${system.charger_can_ready ? 'ready' : 'not ready'} (ID ${system.charger_can_id_hex || '0x0'})`;

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
    input, button {
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
    <p>Upload the compiled ESP32 <code>.bin</code> file from PlatformIO. This updates the dashboard and charger bridge firmware.</p>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="update" accept=".bin" required>
      <button type="submit">Install Update</button>
    </form>
    <a href="/">Back to dashboard</a>
  </main>
</body>
</html>
)HTML";

const char kConfigHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Amp BMS Config</title>
  <style>
    body { margin: 0; min-height: 100vh; display: grid; place-items: center; font-family: "Avenir Next", "Segoe UI", sans-serif; background: #f3eee6; color: #1f2a24; }
    main { width: min(620px, calc(100vw - 28px)); padding: 24px; border-radius: 18px; background: #fffaf2; border: 1px solid rgba(31,42,36,.15); }
    h1 { margin-top: 0; }
    label { display: block; margin-top: 10px; font-size: .9rem; color: #59655d; }
    input, select, button { width: 100%; padding: 10px 12px; border-radius: 10px; border: 1px solid rgba(31,42,36,.2); font: inherit; }
    button { margin-top: 16px; border: none; color: #fff; background: #0f7a5c; font-weight: 700; cursor: pointer; }
    a { color: #0f7a5c; font-weight: 700; text-decoration: none; }
  </style>
</head>
<body>
  <main>
    <h1>Config</h1>
    <form method="POST" action="/config">
      <label>WiFi SSID</label>
      <input name="ssid" value="%SSID%">
      <label>WiFi Password</label>
      <input name="password" value="%PASS%">
      <label>Charger Bridge Enabled</label>
      <select name="charger_enabled">
        <option value="1" %EN1%>Enabled</option>
        <option value="0" %EN0%>Disabled</option>
      </select>
      <label>Elcon CAN ID (hex)</label>
      <input name="can_id" value="%CANID%">
      <label>Elcon CAN Baud (kbps)</label>
      <input name="can_kbps" value="%CANKBPS%">
      <label>MCP2515 Oscillator</label>
      <select name="osc_8mhz">
        <option value="1" %OSC8%>8 MHz</option>
        <option value="0" %OSC16%>16 MHz</option>
      </select>
      <label>Command Period (ms)</label>
      <input name="cmd_period_ms" value="%CMDMS%">
      <label>Telemetry Stale Timeout (ms)</label>
      <input name="stale_ms" value="%STALEMS%">
      <button type="submit">Save & Reboot</button>
    </form>
    <p><a href="/">Back to dashboard</a></p>
  </main>
</body>
</html>
)HTML";

bool parseJsonFloat(const String &json, const char *key, float &outValue) {
  const String token = String("\"") + key + "\":";
  const int keyPos = json.indexOf(token);
  if (keyPos < 0) {
    return false;
  }

  int valueStart = keyPos + token.length();
  while (valueStart < (int)json.length() && (json[valueStart] == ' ' || json[valueStart] == '"')) {
    valueStart++;
  }

  int valueEnd = valueStart;
  while (valueEnd < (int)json.length()) {
    const char c = json[valueEnd];
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
      valueEnd++;
      continue;
    }
    break;
  }

  if (valueEnd <= valueStart) {
    return false;
  }

  outValue = json.substring(valueStart, valueEnd).toFloat();
  return true;
}

bool parseJsonInt(const String &json, const char *key, int &outValue) {
  float value = 0.0f;
  if (!parseJsonFloat(json, key, value)) {
    return false;
  }
  outValue = (int)value;
  return true;
}

bool parseJsonString(const String &json, const char *key, String &outValue) {
  const String token = String("\"") + key + "\":\"";
  const int keyPos = json.indexOf(token);
  if (keyPos < 0) {
    return false;
  }

  const int valueStart = keyPos + token.length();
  const int valueEnd = json.indexOf('"', valueStart);
  if (valueEnd < 0) {
    return false;
  }

  outValue = json.substring(valueStart, valueEnd);
  return true;
}

uint32_t parseUintArg(const String &value, uint32_t fallback) {
  if (value.length() == 0) {
    return fallback;
  }
  return (uint32_t)strtoul(value.c_str(), nullptr, 0);
}

String toHex(uint32_t value) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "0x%08lX", (unsigned long)value);
  return String(buffer);
}

void loadConfig() {
  prefs.begin("ampbms", true);
  gConfig.wifiSsid = prefs.getString("ssid", kWifiSsid);
  gConfig.wifiPassword = prefs.getString("pass", kWifiPassword);
  gConfig.chargerEnabled = prefs.getBool("chg_en", true);
  gConfig.chargerCanId = prefs.getULong("chg_id", kElconCanId);
  gConfig.chargerCanBaudKbps = prefs.getULong("chg_k", kElconCanBaudKbps);
  gConfig.chargerCanOsc8MHz = prefs.getBool("chg_o8", kElconCanOscillator8MHz);
  gConfig.chargerCommandPeriodMs = prefs.getULong("chg_ms", 100);
  gConfig.telemetryStaleMs = prefs.getULong("stale", 2500);
  prefs.end();
}

void saveConfig() {
  prefs.begin("ampbms", false);
  prefs.putString("ssid", gConfig.wifiSsid);
  prefs.putString("pass", gConfig.wifiPassword);
  prefs.putBool("chg_en", gConfig.chargerEnabled);
  prefs.putULong("chg_id", gConfig.chargerCanId);
  prefs.putULong("chg_k", gConfig.chargerCanBaudKbps);
  prefs.putBool("chg_o8", gConfig.chargerCanOsc8MHz);
  prefs.putULong("chg_ms", gConfig.chargerCommandPeriodMs);
  prefs.putULong("stale", gConfig.telemetryStaleMs);
  prefs.end();
}

bool wifiCredentialsConfigured() {
  return gConfig.wifiSsid.length() > 0;
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
  WiFi.begin(gConfig.wifiSsid.c_str(), gConfig.wifiPassword.c_str());

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

bool configureChargerCan() {
  SPI.begin();
  pinMode(kMcp2515IntPin, INPUT_PULLUP);

  chargerCan.reset();

  const CAN_CLOCK clock = gConfig.chargerCanOsc8MHz ? MCP_8MHZ : MCP_16MHZ;
  CAN_SPEED speed = CAN_250KBPS;

  switch (gConfig.chargerCanBaudKbps) {
    case 125:
      speed = CAN_125KBPS;
      break;
    case 250:
      speed = CAN_250KBPS;
      break;
    case 500:
      speed = CAN_500KBPS;
      break;
    default:
      speed = CAN_250KBPS;
      break;
  }

  if (chargerCan.setBitrate(speed, clock) != MCP2515::ERROR_OK) {
    gCharger.errCount++;
    return false;
  }

  if (chargerCan.setNormalMode() != MCP2515::ERROR_OK) {
    gCharger.errCount++;
    return false;
  }

  return true;
}

void updateParsedTelemetry() {
  ParsedTelemetry parsed;
  parsed.valid = true;

  parseJsonFloat(gLatestTelemetry, "charge_target_v", parsed.chargeTargetV);
  parseJsonFloat(gLatestTelemetry, "charge_limit_a", parsed.chargeLimitA);
  parseJsonFloat(gLatestTelemetry, "pack_voltage_v", parsed.packVoltageV);
  parseJsonInt(gLatestTelemetry, "charge_state", parsed.chargeState);
  parseJsonInt(gLatestTelemetry, "status_code", parsed.bmsStatusCode);
  parseJsonString(gLatestTelemetry, "status_text", parsed.bmsStatusText);

  gTelemetry = parsed;
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
        updateParsedTelemetry();
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

void formatCanDataHex(const struct can_frame &frame, String &out) {
  out = "";
  for (int i = 0; i < frame.can_dlc; i++) {
    char byteHex[4];
    snprintf(byteHex, sizeof(byteHex), "%02X", frame.data[i]);
    out += byteHex;
    if (i < frame.can_dlc - 1) {
      out += " ";
    }
  }
}

void pollChargerFeedback() {
  if (!gCharger.canReady) {
    return;
  }

  struct can_frame frame;
  for (int i = 0; i < 4; i++) {
    if (chargerCan.readMessage(&frame) != MCP2515::ERROR_OK) {
      break;
    }

    gCharger.rxCount++;
    gCharger.lastRxMs = millis();
    gCharger.lastRxId = (uint32_t)frame.can_id;
    formatCanDataHex(frame, gCharger.lastRxDataHex);
  }
}

void sendElconCommand(float targetVoltageV, float targetCurrentA) {
  struct can_frame frame;
  memset(&frame, 0, sizeof(frame));
  frame.can_id = (gConfig.chargerCanId & CAN_EFF_MASK) | CAN_EFF_FLAG;
  frame.can_dlc = 8;

  const uint16_t voltageDeciV = (uint16_t)max(0.0f, targetVoltageV * 10.0f);
  const uint16_t currentDeciA = (uint16_t)max(0.0f, targetCurrentA * 10.0f);

  frame.data[0] = (voltageDeciV >> 8) & 0xFF;
  frame.data[1] = voltageDeciV & 0xFF;
  frame.data[2] = (currentDeciA >> 8) & 0xFF;
  frame.data[3] = currentDeciA & 0xFF;
  frame.data[4] = 0x00;
  frame.data[5] = 0x00;
  frame.data[6] = 0x00;
  frame.data[7] = 0x00;

  if (chargerCan.sendMessage(&frame) == MCP2515::ERROR_OK) {
    gCharger.txCount++;
    gCharger.lastCommandMs = millis();
    gCharger.lastCommandV = targetVoltageV;
    gCharger.lastCommandA = targetCurrentA;
    gCharger.commandActive = targetCurrentA > 0.01f;
  } else {
    gCharger.errCount++;
    gCharger.commandActive = false;
  }
}

void serviceChargerBridge() {
  pollChargerFeedback();

  if (!gConfig.chargerEnabled || !gCharger.canReady) {
    return;
  }

  if (millis() - gLastChargerTickMs < gConfig.chargerCommandPeriodMs) {
    return;
  }
  gLastChargerTickMs = millis();

  const bool stale = (gLastTelemetryMs == 0) || (millis() - gLastTelemetryMs > gConfig.telemetryStaleMs);
  gCharger.staleTelemetry = stale;

  float targetVoltage = 0.0f;
  float targetCurrent = 0.0f;

  if (!stale && gTelemetry.valid && gTelemetry.chargeState == 1 && gTelemetry.chargeLimitA > 0.0f && gTelemetry.chargeTargetV > 0.0f) {
    targetVoltage = gTelemetry.chargeTargetV;
    targetCurrent = gTelemetry.chargeLimitA;
  }

  sendElconCommand(targetVoltage, targetCurrent);
}

String escapeHtml(const String &input) {
  String out = input;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
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
  response.reserve(700);
  response += "{\"wifi_mode\":\"";
  response += gWifiMode;
  response += "\",\"ip\":\"";
  response += currentIpAddress();
  response += "\",\"hostname\":\"";
  response += kDashboardHostname;
  response += "\",\"telemetry_age_ms\":";
  response += String(gLastTelemetryMs == 0 ? 0 : millis() - gLastTelemetryMs);
  response += ",\"telemetry_stale_ms\":";
  response += String(gConfig.telemetryStaleMs);
  response += ",\"esp32_uptime_ms\":";
  response += String(millis());
  response += ",\"charger_can_ready\":";
  response += gCharger.canReady ? "true" : "false";
  response += ",\"charger_enabled\":";
  response += gConfig.chargerEnabled ? "true" : "false";
  response += ",\"charger_command_active\":";
  response += gCharger.commandActive ? "true" : "false";
  response += ",\"charger_stale_telemetry\":";
  response += gCharger.staleTelemetry ? "true" : "false";
  response += ",\"charger_can_id_hex\":\"";
  response += toHex(gConfig.chargerCanId);
  response += "\",\"charger_can_kbps\":";
  response += String(gConfig.chargerCanBaudKbps);
  response += ",\"charger_cmd_period_ms\":";
  response += String(gConfig.chargerCommandPeriodMs);
  response += ",\"charger_tx_count\":";
  response += String(gCharger.txCount);
  response += ",\"charger_rx_count\":";
  response += String(gCharger.rxCount);
  response += ",\"charger_err_count\":";
  response += String(gCharger.errCount);
  response += ",\"charger_last_cmd_v\":";
  response += String(gCharger.lastCommandV, 2);
  response += ",\"charger_last_cmd_a\":";
  response += String(gCharger.lastCommandA, 2);
  response += ",\"charger_last_rx_id\":\"";
  response += toHex(gCharger.lastRxId);
  response += "\",\"charger_last_rx_data\":\"";
  response += gCharger.lastRxDataHex;
  response += "\"}";

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

void handleConfigGet() {
  String page = FPSTR(kConfigHtml);
  page.replace("%SSID%", escapeHtml(gConfig.wifiSsid));
  page.replace("%PASS%", escapeHtml(gConfig.wifiPassword));
  page.replace("%EN1%", gConfig.chargerEnabled ? "selected" : "");
  page.replace("%EN0%", gConfig.chargerEnabled ? "" : "selected");
  page.replace("%CANID%", toHex(gConfig.chargerCanId));
  page.replace("%CANKBPS%", String(gConfig.chargerCanBaudKbps));
  page.replace("%OSC8%", gConfig.chargerCanOsc8MHz ? "selected" : "");
  page.replace("%OSC16%", gConfig.chargerCanOsc8MHz ? "" : "selected");
  page.replace("%CMDMS%", String(gConfig.chargerCommandPeriodMs));
  page.replace("%STALEMS%", String(gConfig.telemetryStaleMs));
  server.send(200, "text/html", page);
}

void handleConfigPost() {
  gConfig.wifiSsid = server.arg("ssid");
  gConfig.wifiPassword = server.arg("password");
  gConfig.chargerEnabled = server.arg("charger_enabled") != "0";
  gConfig.chargerCanId = parseUintArg(server.arg("can_id"), gConfig.chargerCanId);
  gConfig.chargerCanBaudKbps = parseUintArg(server.arg("can_kbps"), gConfig.chargerCanBaudKbps);
  gConfig.chargerCanOsc8MHz = server.arg("osc_8mhz") != "0";
  gConfig.chargerCommandPeriodMs = parseUintArg(server.arg("cmd_period_ms"), gConfig.chargerCommandPeriodMs);
  gConfig.telemetryStaleMs = parseUintArg(server.arg("stale_ms"), gConfig.telemetryStaleMs);

  if (gConfig.chargerCommandPeriodMs < 20) {
    gConfig.chargerCommandPeriodMs = 20;
  }
  if (gConfig.telemetryStaleMs < 200) {
    gConfig.telemetryStaleMs = 200;
  }

  saveConfig();
  server.send(200, "text/plain", "Saved. Rebooting in 1 second.");
  delay(1000);
  ESP.restart();
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleDashboard);
  server.on("/api/telemetry", HTTP_GET, handleTelemetry);
  server.on("/api/system", HTTP_GET, handleSystemInfo);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateResult, handleUpdateUpload);
  server.on("/config", HTTP_GET, handleConfigGet);
  server.on("/config", HTTP_POST, handleConfigPost);
  server.on("/favicon.ico", HTTP_GET, []() {
    server.send(204);
  });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  loadConfig();

  gTelemetryLine.reserve(768);
  gLatestTelemetry.reserve(768);
  gCharger.lastRxDataHex.reserve(64);

  teensyLink.begin(kTeensyLinkBaud, SERIAL_8N1, kTeensyLinkRxPin, kTeensyLinkTxPin);

  connectWifi();

  if (MDNS.begin(kDashboardHostname)) {
    MDNS.addService("http", "tcp", 80);
  }

  gCharger.canReady = configureChargerCan();
  setupWebServer();
}

void loop() {
  readTeensyTelemetry();
  serviceChargerBridge();
  server.handleClient();
}

