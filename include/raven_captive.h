#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "raven_config.h"
#include "raven_device.h"

// ── State ─────────────────────────────────────────────────────────────────────
static DNSServer        dnsServer;
static AsyncWebServer   apServer(80);
static bool             captiveActive = false;
static bool             setupComplete = false;  // set true after portal save+reboot

// ── Setup page HTML ───────────────────────────────────────────────────────────
// Intentionally lean — loads on phones with no internet access
static const char SETUP_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bird Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#0d0f14;color:#e2e8f0;min-height:100vh;
     display:flex;align-items:center;justify-content:center;padding:20px}
.card{background:#1a1f2e;border:1px solid #2d3748;border-radius:12px;padding:28px;
      width:100%;max-width:420px}
h1{font-size:22px;font-weight:700;color:#fff;margin-bottom:4px}
.sub{font-size:13px;color:#718096;margin-bottom:24px}
.section{font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.1em;
         color:#7c6af7;margin:20px 0 10px}
label{display:block;font-size:12px;color:#a0aec0;margin-bottom:4px}
input{width:100%;background:#0d0f14;border:1px solid #2d3748;border-radius:6px;
      color:#e2e8f0;padding:9px 12px;font-size:14px;margin-bottom:12px}
input:focus{outline:none;border-color:#7c6af7}
.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.preview{background:#0d0f14;border:1px solid #2d3748;border-radius:6px;
         padding:8px 12px;font-size:12px;font-family:monospace;color:#68d391;
         margin-bottom:16px;word-break:break-all}
button{width:100%;background:#7c6af7;border:none;border-radius:7px;color:#fff;
       font-size:15px;font-weight:600;padding:12px;cursor:pointer;margin-top:4px}
button:hover{filter:brightness(1.1)}
button:active{transform:scale(.98)}
.msg{display:none;margin-top:14px;padding:10px 14px;border-radius:7px;font-size:13px}
.msg.ok{background:#1a3a2a;color:#68d391;border:1px solid #276749}
.msg.err{background:#3a1a1a;color:#fc8181;border:1px solid #742a2a}
.spinner{display:none;text-align:center;margin-top:14px;color:#7c6af7;font-size:13px}
</style>
</head>
<body>
<div class="card">
  <h1>🐦 Bird Setup</h1>
  <div class="sub">Configure WiFi and show control settings</div>

  <div class="section">Device Identity</div>
  <label>Device Name (display label)</label>
  <input type="text" id="device_name" placeholder="Raven 1" maxlength="32">
  <label>Hostname (for http://______.local)</label>
  <input type="text" id="mdns_hostname" placeholder="raven1" maxlength="32"
         oninput="this.value=this.value.toLowerCase().replace(/[^a-z0-9-]/g,'');updatePreview()">

  <div class="section">MQTT Topic Hierarchy</div>
  <label>Location (top level)</label>
  <input type="text" id="mqtt_location" placeholder="props" maxlength="32"
         oninput="sanitize(this);updatePreview()">
  <label>Room</label>
  <input type="text" id="mqtt_room" placeholder="study" maxlength="32"
         oninput="sanitize(this);updatePreview()">
  <label>Device ID (used in topic)</label>
  <input type="text" id="mqtt_device" placeholder="raven1" maxlength="32"
         oninput="sanitize(this);updatePreview()">
  <label style="margin-bottom:6px">Full MQTT prefix preview</label>
  <div class="preview" id="preview">props/study/raven1</div>

  <div class="section">MQTT Broker</div>
  <label>Broker address (hostname or IP)</label>
  <input type="text" id="mqtt_server" placeholder="homeassistant.local">
  <label>Port</label>
  <input type="number" id="mqtt_port" value="1883" min="1" max="65535">

  <div class="section">WiFi</div>
  <label>Network SSID</label>
  <input type="text" id="wifi_ssid" placeholder="Your WiFi name">
  <label>Password</label>
  <input type="password" id="wifi_password" placeholder="WiFi password">

  <button onclick="save()">Save & Connect</button>
  <div class="msg" id="msg"></div>
  <div class="spinner" id="spin">Saving... device will reboot and join your network.</div>
</div>
<script>
function sanitize(el){el.value=el.value.toLowerCase().replace(/[^a-z0-9_-]/g,'');}
function updatePreview(){
  const loc=document.getElementById('mqtt_location').value||'props';
  const room=document.getElementById('mqtt_room').value||'room';
  const dev=document.getElementById('mqtt_device').value||'device';
  document.getElementById('preview').textContent=loc+'/'+room+'/'+dev;
}
function showMsg(text,ok){
  const el=document.getElementById('msg');
  el.className='msg '+(ok?'ok':'err');
  el.textContent=text;
  el.style.display='block';
}
function save(){
  const fields=['device_name','mdns_hostname','mqtt_location','mqtt_room',
                'mqtt_device','mqtt_server','mqtt_port','wifi_ssid','wifi_password'];
  const data={};
  for(const f of fields) data[f]=document.getElementById(f).value.trim();
  if(!data.wifi_ssid){showMsg('WiFi SSID is required','err');return;}
  if(!data.mqtt_server){showMsg('MQTT broker address is required','err');return;}
  if(!data.mqtt_location||!data.mqtt_room||!data.mqtt_device){
    showMsg('Location, Room, and Device ID are all required','err');return;}
  data.mqtt_port=parseInt(data.mqtt_port)||1883;
  document.getElementById('spin').style.display='block';
  fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(data)})
    .then(r=>r.json())
    .then(d=>{
      if(d.ok){showMsg('Saved! Device is rebooting — connect to your WiFi network and navigate to http://'+data.mdns_hostname+'.local','ok');}
      else showMsg('Save failed: '+d.error,'err');
      document.getElementById('spin').style.display='none';
    })
    .catch(()=>{
      showMsg('Saved — device rebooting. Connect to your WiFi and navigate to http://'+
              (document.getElementById('mdns_hostname').value||'bird1')+'.local','ok');
      document.getElementById('spin').style.display='none';
    });
}
updatePreview();
</script>
</body>
</html>
)HTML";

// ── WiFi connection ───────────────────────────────────────────────────────────
bool wifiConnect() {
    if (!device.isConfigured()) return false;
    Serial.printf("[wifi] Connecting to %s", device.wifi_ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(device.wifi_ssid.c_str(), device.wifi_password.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println("\n[wifi] Timeout");
            return false;
        }
        delay(300);
        Serial.print(".");
    }
    Serial.printf("\n[wifi] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

// ── WiFi auto-reconnect ───────────────────────────────────────────────────────
// Called from loop() — reconnects with exponential backoff if WiFi drops.
// Backoff: 5s → 10s → 20s → 40s → 60s (cap) to avoid hammering the AP.
void wifiReconnectLoop() {
    static unsigned long lastAttempt  = 0;
    static unsigned long backoffMs    = 5000;
    static bool          wasConnected = false;

    bool connected = (WiFi.status() == WL_CONNECTED);

    if (connected) {
        if (!wasConnected) {
            Serial.printf("[wifi] Reconnected — IP: %s\n",
                          WiFi.localIP().toString().c_str());
        }
        wasConnected = true;
        backoffMs    = 5000;    // reset backoff on successful connection
        return;
    }

    // Not connected
    if (wasConnected) {
        Serial.println("[wifi] Connection lost — will retry");
        wasConnected = false;
    }

    if (millis() - lastAttempt >= backoffMs) {
        lastAttempt = millis();
        Serial.printf("[wifi] Reconnect attempt (backoff %lums)...\n", backoffMs);
        WiFi.disconnect();
        WiFi.begin(device.wifi_ssid.c_str(), device.wifi_password.c_str());
        // Increase backoff, cap at 60s
        backoffMs = min(backoffMs * 2, (unsigned long)60000);
    }
}

// ── Captive portal ────────────────────────────────────────────────────────────
void captiveStart() {
    captiveActive = true;
    Serial.printf("[captive] Starting AP: %s\n", AP_SSID);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, strlen(AP_PASSWORD) > 0 ? AP_PASSWORD : nullptr);
    delay(100);

    IPAddress apIP;
    apIP.fromString(AP_IP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    // DNS — redirect everything to the ESP32 so any URL triggers the portal
    dnsServer.start(53, "*", apIP);
    Serial.printf("[captive] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // Serve setup page for all paths (captive portal redirect)
    apServer.onNotFound([](AsyncWebServerRequest* req) {
        req->send(200, "text/html", SETUP_HTML);
    });
    apServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", SETUP_HTML);
    });

    // iOS / Android captive portal detection endpoints — redirect to setup page
    apServer.on("/generate_204",       HTTP_GET, [](AsyncWebServerRequest* r){ r->redirect("/"); });
    apServer.on("/hotspot-detect.html",HTTP_GET, [](AsyncWebServerRequest* r){ r->redirect("/"); });
    apServer.on("/ncsi.txt",           HTTP_GET, [](AsyncWebServerRequest* r){ r->redirect("/"); });
    apServer.on("/connecttest.txt",    HTTP_GET, [](AsyncWebServerRequest* r){ r->redirect("/"); });
    apServer.on("/fwlink",             HTTP_GET, [](AsyncWebServerRequest* r){ r->redirect("/"); });

    // Save endpoint
    apServer.on("/save", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
                return;
            }
            deviceFromJson(doc);
            if (deviceConfigSave()) {
                req->send(200, "application/json", "{\"ok\":true}");
                Serial.println("[captive] Config saved — rebooting");
                delay(1000);
                ESP.restart();
            } else {
                req->send(500, "application/json", "{\"ok\":false,\"error\":\"save failed\"}");
            }
        }
    );

    apServer.begin();
    Serial.println("[captive] Portal ready — connect to BirdSetup AP");
}

void captiveLoop() {
    if (captiveActive) dnsServer.processNextRequest();
}
