/**
 * Hidroponik Otomatis dengan Monitoring Web Internal & MQTT
 *
 * Menggabungkan logika kontrol hidroponik (sensor, pompa, MQTT)
 * dengan server web internal (AsyncWebServer + WebSocket) untuk
 * monitoring dan kontrol via browser di jaringan lokal.
 *
 * v7:
 * - [PERBAIKAN] Menambahkan flag 'tdsBufferReady' untuk mencegah error TDS palsu
 * segera setelah periode pemanasan sensor selesai. Error TDS (<=0) hanya
 * akan dilaporkan setelah buffer sensor TDS terisi penuh setidaknya sekali.
 * - (Fitur v6 dipertahankan: WiFi Exception Handling, DHCP Mode)
 */

#include <WiFi.h>
#include <WiFiClientSecure.h> 
#include <PubSubClient.h>     
#include <ArduinoJson.h>      
#include <WiFiManager.h>      
#include <OneWire.h>          
#include <DallasTemperature.h> 
#include <time.h>             
#include <Preferences.h>      
#include <ESPAsyncWebServer.h> 
#include <AsyncTCP.h>          


const char* mqtt_server = "eec79f8e.ala.asia-southeast1.emqxsl.com";
const int mqtt_port = 8883;
const char* mqtt_user = "Hidro_IoT";
const char* mqtt_pass = "Tult0612_labiottel-U";

const char* web_password = "admin"; 

#define PH_PIN 4
#define TdsSensorPin 5
#define ONE_WIRE_BUS 20
const int RELAY_CH1_PIN = 15; 
const int RELAY_CH2_PIN = 16; 
const int RELAY_CH3_PIN = 17; 
const int RELAY_CH4_PIN = 18; 

#define VREF 3.30        
#define SCOUNT 30        
const unsigned long DURASI_PULSE_POMPA = 500; 
const unsigned long sensorWarmUpTime = 30000; 

const long publishInterval = 10000;      
const long helloInterval = 5000;         
const long mixingInterval = 60000;       
const long errorPublishInterval = 30000; 
const long mqttReconnectInterval = 5000; 
const long wifiRestartTimeout = 300000;  
const long dailyCheckInterval = 30000;   

#define NVS_NAMESPACE "hidroponik"
#define KEY_SSID "ssid"
#define KEY_PASS "password"
#define KEY_ACID_VOLT "acidVolt"
#define KEY_NEUTRAL_VOLT "neutralVolt"
#define KEY_ALKALINE_VOLT "alkalineVolt"
#define KEY_TDS_KOREKSI "koreksiTDS"
#define KEY_PH_MIN "ph_min"
#define KEY_PH_MAX "ph_max"
#define KEY_PPM_MIN "ppm_min"
#define KEY_PPM_MAX "ppm_max"
#define KEY_WIFI_ATTEMPT "wifiAttempt" 

const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb29tIEcyMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n" \
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n" \
"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n" \
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n" \
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n" \
"MrY=\n" \
"-----END CERTIFICATE-----\n";


char device_id[20] = "EHT-25A10001";
char device_mode[10] = "idle";
char crop_cycle_id[40] = "";

char mqtt_topic[100];
char command_topic[100];
char hello_topic[100];
char response_topic[100];

float acidVoltage = 1985.0;
float neutralVoltage = 1480.0;
float alkalineVoltage = 1000.0;
float FAKTOR_KOREKSI = 1.0;
float PH_LOW_LIMIT = 6.0;
float PH_HIGH_LIMIT = 8.0;
float PPM_LOW_LIMIT = 600.0;
float PPM_HIGH_LIMIT = 800.0;

float voltage, phValue = 7.0;
float tdsValue = 0, temperature = 25.0;
int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0;
float averageVoltage = 0;

bool tempErrorState = false;
bool phErrorState = false;
bool tdsErrorState = false;
bool tdsBufferReady = false; 

unsigned long lastPublishTime = 0;
unsigned long lastHelloTime = 0;
unsigned long lastDoseTime = 0;
unsigned long cycleStartTime = 0;
unsigned long lastErrorPublishTime = 0;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long firstWifiDisconnectTime = 0;
unsigned long lastTimeCheck = 0;

bool isPompaPhOn = false;
bool isPompaNutrisiOn = false;
unsigned long pompaPhStartTime = 0;
unsigned long pompaNutrisiStartTime = 0;

bool sudahRestartHariIni = false;

bool webUserLoggedIn = false; 

bool flagResetWifi = false; 
bool flagRestart = false;   


WiFiManager wm;
WiFiClientSecure wifiClient; 
PubSubClient client(wifiClient);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Preferences preferences;
AsyncWebServer server(80);     
AsyncWebSocket ws("/ws");      


const char loginPageHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Login - Hidroponik Monitor</title>
    <style>
        body { font-family: sans-serif; background-color: #1f2b38; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
        .login-container { background-color: #2c3a47; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); width: 300px; }
        h2 { text-align: center; color: #ecf0f1; margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; color: #bdc3c7; }
        input[type="password"] { width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #7f8c8d; background-color: #4a5562; color: #ecf0f1; border-radius: 4px; box-sizing: border-box; }
        button { width: 100%; background-color: #5cb85c; color: white; padding: 12px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        button:hover { background-color: #4cae4c; }
        .error { color: #e74c3c; text-align: center; margin-top: 10px; font-size: 0.9em; }
    </style>
</head>
<body>
    <div class="login-container">
        <h2>Hidroponik Monitor</h2>
        <form action="/login" method="post">
            <label for="password">Password:</label>
            <input type="password" id="password" name="password" required>
            <button type="submit">Login</button>
        </form>
        <div class="error" id="error-msg"></div>
    </div>
    <script>
        const urlParams = new URLSearchParams(window.location.search);
        if (urlParams.has('error')) {
            document.getElementById('error-msg').textContent = 'Password salah!';
        }
    </script>
</body>
</html>
)rawliteral";

const char dashboardPageHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dashboard Hidroponik</title>
    <style>
        body { font-family: sans-serif; background-color: #1f2b38; color: #ecf0f1; margin: 0; padding: 20px; }
        h1, h2 { color: #1abc9c; }
        .container { max-width: 1000px; margin: auto; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; margin-bottom: 20px;}
        .card { background-color: #2c3a47; padding: 20px; border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.2); }
        .card h3 { margin-top: 0; border-bottom: 1px solid #3e5062; padding-bottom: 10px; margin-bottom: 15px; }
        .card .value { font-size: 2em; font-weight: bold; color: #1abc9c; }
        .card .unit { font-size: 1em; color: #bdc3c7; }
        .status { font-weight: bold; }
        .status.online { color: #2ecc71; }
        .status.offline { color: #e74c3c; }
        .status.idle { color: #f1c40f; }
        .status.active { color: #2ecc71; }
        .status.error { color: #e74c3c; }
        button { background-color: #3498db; color: white; padding: 8px 15px; border: none; border-radius: 4px; cursor: pointer; margin-right: 5px; }
        button:hover { background-color: #2980b9; }
        button.danger { background-color: #e74c3c; }
        button.danger:hover { background-color: #c0392b; }
        input[type="number"], input[type="text"], input[type="password"] { padding: 8px; border-radius: 4px; border: 1px solid #7f8c8d; background-color: #4a5562; color: #ecf0f1; margin-bottom: 5px;}
        label { display: block; margin-bottom: 3px; font-size: 0.9em; color: #bdc3c7;}
        .form-group { margin-bottom: 15px; }
        hr { border: none; border-top: 1px solid #3e5062; margin: 20px 0; }
        #error-banner { background-color: #c0392b; padding: 10px; border-radius: 4px; margin-top: 10px; display: none; }
        #log-panel { background-color: #1a1a1a; padding: 10px; border-radius: 4px; height: 150px; overflow-y: scroll; font-family: monospace; font-size: 0.8em; margin-top: 10px; }
        #log-panel div { margin-bottom: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Hidroponik Dashboard</h1>
        <p>Device ID: <span id="device-id">EHT-25A10001</span> | Status: <span id="ws-status" class="status offline">Offline</span></p>

        <div class="grid">
            <div class="card">
                <h3>Status Sistem</h3>
                <p>Mode: <span id="status-mode" class="status idle">Idle</span></p>
                <p>Cycle ID: <span id="status-cycle">--</span></p>
                <p>Update Terakhir: <span id="status-timestamp">--</span></p>
            </div>
            <div class="card">
                <h3>pH Air</h3>
                <span id="ph-value" class="value">--</span>
            </div>
            <div class="card">
                <h3>TDS</h3>
                <span id="ppm-value" class="value">--</span> <span class="unit">ppm</span>
            </div>
             <div class="card">
                <h3>Suhu Air</h3>
                <span id="temp-value" class="value">--</span> <span class="unit">°C</span>
            </div>
        </div>

        <div id="error-banner">
            <strong>Sensor Failure!</strong> <span id="error-text"></span>
        </div>

        <div class="card">
              <h3>Kontrol Perangkat</h3>
              <div class="form-group">
                  <h4>Sync Threshold (Saat Cycle Berjalan)</h4>
                  <label for="ph_min">pH Min:</label> <input type="number" step="0.1" id="ph_min" placeholder="6.0">
                  <label for="ph_max">pH Max:</label> <input type="number" step="0.1" id="ph_max" placeholder="8.0"><br>
                  <label for="ppm_min">PPM Min:</label> <input type="number" id="ppm_min" placeholder="600">
                  <label for="ppm_max">PPM Max:</label> <input type="number" id="ppm_max" placeholder="800"><br>
                  <button onclick="sendSyncThreshold()">Kirim Threshold</button>
              </div>
              <hr>
               <div class="form-group">
                  <h4>Kontrol Sesi</h4>
                  <div class="form-group" id="start-cycle-form">
                    <label for="cycle_id">Cycle ID Baru:</label>
                    <input type="text" id="cycle_id" placeholder="Contoh: Sawi-Batch-01" style="width: 95%;">
                    <label for="start_ph_min">pH Min:</label> <input type="number" step="0.1" id="start_ph_min" placeholder="6.0">
                    <label for="start_ph_max">pH Max:</label> <input type="number" step="0.1" id="start_ph_max" placeholder="8.0"><br>
                    <label for="start_ppm_min">PPM Min:</label> <input type="number" id="start_ppm_min" placeholder="600">
                    <label for="start_ppm_max">PPM Max:</label> <input type="number" id="start_ppm_max" placeholder="800"><br>
                    <button onclick="sendStartCycle()" style="background-color: #2ecc71; margin-bottom: 10px; width: 95%;">START CYCLE BARU</button>
                  </div>
                  <button onclick="sendStopCycle()" class="danger" style="width: 95%;">STOP CYCLE SAAT INI</button>
               </div>
              <hr>
               <div class="form-group">
                  <h4>Kontrol Jaringan</h4>
                  <label for="wifi_ssid">SSID Baru:</label> <input type="text" id="wifi_ssid" placeholder="Nama WiFi">
                  <label for="wifi_pass">Password Baru:</label> <input type="password" id="wifi_pass" placeholder="Password"><br>
                  <button onclick="sendSetWifi()">Ganti WiFi & Restart</button>
                  <button onclick="sendResetWifi()" class="danger">RESET WIFI (AP Mode)</button>
               </div>
        </div>

         <div class="card">
            <h3>Log Sistem</h3>
            <div id="log-panel"></div>
         </div>
    </div>

    <script>
        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket;

        const wsStatus = document.getElementById('ws-status');
        const statusMode = document.getElementById('status-mode');
        const statusCycle = document.getElementById('status-cycle');
        const statusTimestamp = document.getElementById('status-timestamp');
        const phValue = document.getElementById('ph-value');
        const ppmValue = document.getElementById('ppm-value');
        const tempValue = document.getElementById('temp-value');
        const errorBanner = document.getElementById('error-banner');
        const errorText = document.getElementById('error-text');
        const logPanel = document.getElementById('log-panel');
        const phMinInput = document.getElementById('ph_min');
        const phMaxInput = document.getElementById('ph_max');
        const ppmMinInput = document.getElementById('ppm_min');
        const ppmMaxInput = document.getElementById('ppm_max');
        const wifiSsidInput = document.getElementById('wifi_ssid');
        const wifiPassInput = document.getElementById('wifi_pass');
        const cycleIdInput = document.getElementById('cycle_id');
        const startPhMinInput = document.getElementById('start_ph_min');
        const startPhMaxInput = document.getElementById('start_ph_max');
        const startPpmMinInput = document.getElementById('start_ppm_min');
        const startPpmMaxInput = document.getElementById('start_ppm_max');

        function initWebSocket() {
            console.log('Mencoba koneksi WebSocket...');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage;
            websocket.onerror = onError;
        }

        function onOpen(event) {
            console.log('Koneksi WebSocket terbuka.');
            wsStatus.textContent = 'Online';
            wsStatus.className = 'status online';
            addToLog('Terhubung ke perangkat.');
        }

        function onClose(event) {
            console.log('Koneksi WebSocket tertutup.');
            wsStatus.textContent = 'Offline';
            wsStatus.className = 'status offline';
            addToLog('Koneksi ke perangkat terputus. Mencoba lagi...');
            setTimeout(initWebSocket, 2000); 
            statusMode.textContent = 'Unknown';
            statusCycle.textContent = '--';
            phValue.textContent = '--';
            ppmValue.textContent = '--';
            tempValue.textContent = '--';
            errorBanner.style.display = 'none';
        }

        function onMessage(event) {
            console.log('Pesan WS diterima:', event.data);
            try {
                const data = JSON.parse(event.data);
                
                if (data.type === 'status') {
                    statusMode.textContent = data.mode.charAt(0).toUpperCase() + data.mode.slice(1);
                    statusMode.className = 'status ' + data.mode; 
                    statusCycle.textContent = data.crop_cycle_id || '--';
                    statusTimestamp.textContent = new Date(data.timestamp).toLocaleString('id-ID');
                    errorBanner.style.display = 'none'; 
                    
                    if(data.ph_min) phMinInput.value = data.ph_min;
                    if(data.ph_max) phMaxInput.value = data.ph_max;
                    if(data.ppm_min) ppmMinInput.value = data.ppm_min;
                    if(data.ppm_max) ppmMaxInput.value = data.ppm_max;
                    
                    if(data.mode === 'idle') {
                        phValue.textContent = '--';
                        ppmValue.textContent = '--';
                        tempValue.textContent = '--';
                        document.getElementById('start-cycle-form').style.display = 'block';
                    } else {
                        document.getElementById('start-cycle-form').style.display = 'none';
                    }
                } else if (data.type === 'data') {
                    phValue.textContent = data.ph.toFixed(2);
                    ppmValue.textContent = data.ppm.toFixed(0);
                    tempValue.textContent = data.temperature.toFixed(2);
                    statusTimestamp.textContent = new Date(data.timestamp).toLocaleString('id-ID');
                    errorBanner.style.display = 'none'; 
                } else if (data.type === 'response') {
                    addToLog(`Respon Perangkat: ${data.message} (${data.status})`);
                } else if (data.type === 'error_report') {
                    const errorMessages = data.sensor_errors.join(', ');
                    errorText.textContent = errorMessages;
                    errorBanner.style.display = 'block';
                    addToLog(`ERROR: ${errorMessages}`, 'error');
                    phValue.textContent = '--';
                    ppmValue.textContent = '--';
                    tempValue.textContent = '--';
                }
            } catch (e) {
                console.error('Gagal parse JSON dari WS:', e);
                addToLog('Menerima data tidak valid dari perangkat.', 'error');
            }
        }
        
        function onError(event) {
             console.error('WebSocket Error:', event);
             addToLog('Terjadi error WebSocket.', 'error');
        }

        function sendCommand(commandData) {
            if (websocket && websocket.readyState === WebSocket.OPEN) {
                const commandString = JSON.stringify(commandData);
                console.log('Mengirim perintah WS:', commandString);
                websocket.send(commandString);
                addToLog(`Mengirim perintah: ${commandData.action || 'unknown'}`);
          } else {
                addToLog('Koneksi WebSocket tidak terbuka. Perintah tidak terkirim.', 'error');
            }
        }
        
        function addToLog(message, type = 'info') {
             const logEntry = document.createElement('div');
             logEntry.textContent = `[${new Date().toLocaleTimeString('id-ID')}] ${message}`;
             if(type === 'error') {
                 logEntry.style.color = '#e74c3c';
             }
             logPanel.insertBefore(logEntry, logPanel.firstChild);
             while (logPanel.childElementCount > 50) {
                 logPanel.removeChild(logPanel.lastChild);
             }
        }

        function sendSyncThreshold() {
            const ph_min = parseFloat(phMinInput.value);
            const ph_max = parseFloat(phMaxInput.value);
            const ppm_min = parseFloat(ppmMinInput.value);
            const ppm_max = parseFloat(ppmMaxInput.value);

            if (isNaN(ph_min) || isNaN(ph_max) || isNaN(ppm_min) || isNaN(ppm_max)) {
                addToLog("Input threshold tidak valid.", "error"); return;
            }
            sendCommand({ action: "sync_threshold", ph_min, ph_max, ppm_min, ppm_max });
        }

        function sendStartCycle() {
            const crop_cycle_id = cycleIdInput.value;
            const ph_min = parseFloat(startPhMinInput.value);
            const ph_max = parseFloat(startPhMaxInput.value);
            const ppm_min = parseFloat(startPpmMinInput.value);
            const ppm_max = parseFloat(startPpmMaxInput.value);

            if (!crop_cycle_id) {
                addToLog("Cycle ID harus diisi.", "error"); return;
            }
            if (isNaN(ph_min) || isNaN(ph_max) || isNaN(ppm_min) || isNaN(ppm_max)) {
                addToLog("Input threshold untuk start cycle tidak valid.", "error"); return;
            }
            if (confirm(`Mulai cycle baru "${crop_cycle_id}"?`)) {
                sendCommand({ 
                    action: "start_cycle", 
                    crop_cycle_id, 
                    ph_min, ph_max, 
                    ppm_min, ppm_max 
                });
            }
        }

        function sendStopCycle() {
            if (confirm("Hentikan siklus saat ini?")) {
                sendCommand({ action: "stop_cycle" });
            }
        }

        function sendSetWifi() {
             const ssid = wifiSsidInput.value;
             const password = wifiPassInput.value;
             if (!ssid || !password) {
                 addToLog("SSID dan Password baru harus diisi.", "error"); return;
             }
             if (confirm(`Ganti WiFi ke "${ssid}"? Perangkat akan restart.`)) {
                 sendCommand({ action: "set_wifi", ssid, password });
             }
         }

         function sendResetWifi() {
             if (confirm("HAPUS SEMUA WiFi tersimpan? Perangkat akan restart ke mode AP.")) {
                 sendCommand({ action: "RESET_WIFI" });
             }
         }

        window.onload = initWebSocket;

    </script>
</body>
</html>
)rawliteral";


String getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Gagal mendapatkan waktu lokal, timestamp mungkin salah.");
        return "1970-01-01T00:00:00Z";
    }
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(timeStr);
}

int getMedianNum(int bArray[], int iFilterLen) {
    int bTab[iFilterLen];
    for (byte i = 0; i < iFilterLen; i++) {
        bTab[i] = bArray[i];
    }
    int i, j, bTemp;
    for (j = 0; j < iFilterLen - 1; j++) {
        for (i = 0; i < iFilterLen - j - 1; i++) {
            if (bTab[i] > bTab[i + 1]) {
                bTemp = bTab[i];
                bTab[i] = bTab[i + 1];
                bTab[i + 1] = bTemp;
            }
        }
    }
    if ((iFilterLen & 1) > 0) {
        bTemp = bTab[(iFilterLen - 1) / 2];
    } else {
        bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
    }
    return bTemp;
}

void saveThresholds() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putFloat(KEY_PH_MIN, PH_LOW_LIMIT);
    preferences.putFloat(KEY_PH_MAX, PH_HIGH_LIMIT);
    preferences.putFloat(KEY_PPM_MIN, PPM_LOW_LIMIT);
    preferences.putFloat(KEY_PPM_MAX, PPM_HIGH_LIMIT);
    preferences.end();
    Serial.println("Threshold baru berhasil DISIMPAN ke NVS.");
}

void savePhCalibration() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putFloat(KEY_ACID_VOLT, acidVoltage);
    preferences.putFloat(KEY_NEUTRAL_VOLT, neutralVoltage);
    preferences.putFloat(KEY_ALKALINE_VOLT, alkalineVoltage);
    preferences.end();
    Serial.println("Kalibrasi pH baru berhasil DISIMPAN ke NVS.");
}

void saveTdsCalibration() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putFloat(KEY_TDS_KOREKSI, FAKTOR_KOREKSI);
    preferences.end();
    Serial.println("Kalibrasi TDS baru berhasil DISIMPAN ke NVS.");
}

void loadSettings() {
    preferences.begin(NVS_NAMESPACE, true);
    acidVoltage = preferences.getFloat(KEY_ACID_VOLT, 1985.0);
    neutralVoltage = preferences.getFloat(KEY_NEUTRAL_VOLT, 1480.0);
    alkalineVoltage = preferences.getFloat(KEY_ALKALINE_VOLT, 1000.0);
    FAKTOR_KOREKSI = preferences.getFloat(KEY_TDS_KOREKSI, 1.0);
    PH_LOW_LIMIT = preferences.getFloat(KEY_PH_MIN, 6.0);
    PH_HIGH_LIMIT = preferences.getFloat(KEY_PH_MAX, 8.0);
    PPM_LOW_LIMIT = preferences.getFloat(KEY_PPM_MIN, 600.0);
    PPM_HIGH_LIMIT = preferences.getFloat(KEY_PPM_MAX, 800.0);
    preferences.end();
    Serial.println("Pengaturan terakhir berhasil dimuat dari NVS.");
    printStatus();
}

void setupPins() {
    pinMode(RELAY_CH1_PIN, OUTPUT); digitalWrite(RELAY_CH1_PIN, HIGH);
    pinMode(RELAY_CH2_PIN, OUTPUT); digitalWrite(RELAY_CH2_PIN, HIGH);
    pinMode(RELAY_CH3_PIN, OUTPUT); digitalWrite(RELAY_CH3_PIN, HIGH);
    pinMode(RELAY_CH4_PIN, OUTPUT); digitalWrite(RELAY_CH4_PIN, HIGH);
    sensors.begin();
    Serial.println("Inisialisasi Pin Selesai.");
}


/**
 * @brief [MODIFIKASI v6] setupWifi() dengan Exception Handling.
 * Jika koneksi gagal setelah perintah 'set_wifi', 
 * perangkat akan menghapus kredensial buruk dan langsung masuk ke AP Mode.
 */
void setupWifi() {
    WiFi.mode(WIFI_STA);

    Serial.println("Membaca kredensial WiFi dari NVS...");
    preferences.begin(NVS_NAMESPACE, false); 
    String saved_ssid = preferences.getString(KEY_SSID, "");
    String saved_pass = preferences.getString(KEY_PASS, "");
    
    bool isNewWifiAttempt = preferences.getBool(KEY_WIFI_ATTEMPT, false);
    if (isNewWifiAttempt) {
        Serial.println("Terdeteksi upaya koneksi WiFi baru (dari set_wifi).");
        preferences.remove(KEY_WIFI_ATTEMPT); 
    }
    
    preferences.end(); 

    if (saved_ssid.length() > 0) {
        Serial.print("Mencoba terhubung ke WiFi tersimpan: "); Serial.println(saved_ssid);
        WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
        
        int connect_timeout = 0;
        while (WiFi.status() != WL_CONNECTED && connect_timeout < 40) { 
            delay(500); Serial.print("."); connect_timeout++; 
        }
        Serial.println();
    }

    if (WiFi.status() != WL_CONNECTED) {
        
        String ap_name = "Setup-Hidroponik-" + String(device_id).substring(strlen(device_id) - 6);
        String serial_display_html = "<p>ID Perangkat Anda (Serial):<br/><b>" + String(device_id) + "</b></p><p>Harap catat ID ini untuk mendaftarkan perangkat di aplikasi Anda.</p>";
        WiFiManagerParameter custom_serial_display("custom_html", serial_display_html.c_str(), "text/html", 200, "style='width:100%;'");
        wm.addParameter(&custom_serial_display);
        wm.setConfigPortalTimeout(300); 

        bool portalRunning;

        if (isNewWifiAttempt) {
            Serial.println("Gagal terhubung dengan kredensial baru. Memaksa BUKA AP Mode (startConfigPortal)...");
            
            preferences.begin(NVS_NAMESPACE, false);
            preferences.remove(KEY_SSID);
            preferences.remove(KEY_PASS);
            preferences.end();
            Serial.println("Kredensial buruk (SSID/Pass) telah dihapus dari NVS.");
            
            portalRunning = wm.startConfigPortal(ap_name.c_str());

        } else {
            Serial.println("Gagal terhubung ke WiFi tersimpan, menjalankan WiFiManager AP (autoConnect)...");
            portalRunning = wm.autoConnect(ap_name.c_str());
        }

        if (!portalRunning) { 
            Serial.println("Gagal terhubung atau setup dibatalkan/timeout. Restart...");
            delay(3000);
            ESP.restart();
        }
        
        Serial.println("Terhubung via WiFiManager. Menyimpan kredensial baru ke NVS...");
        preferences.begin(NVS_NAMESPACE, false);
        preferences.putString(KEY_SSID, WiFi.SSID());
        preferences.putString(KEY_PASS, WiFi.psk());
        preferences.end();
    }
    
    Serial.println("\nWiFi terhubung!");
    Serial.print("Alamat IP (didapat dari DHCP): "); Serial.println(WiFi.localIP());  
}


void setupNtp() {
    Serial.println("Sinkronisasi waktu NTP...");
    configTime(7 * 3600, 0, "pool.ntp.org", "id.pool.ntp.org");
    
    time_t now = time(nullptr);
    int ntp_retries = 0;
    while (now < 1672531200 && ntp_retries < 30) { 
        delay(500); 
        Serial.print("."); 
        now = time(nullptr);
        ntp_retries++;
    }

    if (now < 1672531200) {
        Serial.println("\nPERINGATAN: Gagal sinkronisasi NTP! Waktu mungkin salah.");
        Serial.println("Pastikan perangkat memiliki koneksi internet dan DNS berfungsi.");
    } else {
        Serial.println("\nWaktu berhasil disinkronisasi.");
        struct tm timeinfo;
        if(getLocalTime(&timeinfo)){
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%A, %d %B %Y %H:%M:%S", &timeinfo);
            Serial.print("Waktu saat ini (WIB): "); Serial.println(timeStr);
        } else {
            Serial.println("Gagal mendapatkan waktu lokal");
        }
    }
}

void setupMqtt() {
    sprintf(mqtt_topic, "device/%s/data", device_id);
    sprintf(command_topic, "device/%s/command", device_id);
    sprintf(hello_topic, "device/%s/hello", device_id);
    sprintf(response_topic, "device/%s/response", device_id);
    Serial.println("================================");
    Serial.println("Konfigurasi MQTT:");
    Serial.print("    Device Serial: "); Serial.println(device_id);
    Serial.print("    Topik Data: "); Serial.println(mqtt_topic);
    Serial.print("    Topik Perintah: "); Serial.println(command_topic);
    Serial.print("    Topik Hello: "); Serial.println(hello_topic);
    Serial.print("    Topik Response: "); Serial.println(response_topic);
    wifiClient.setCACert(ca_cert);
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback); 
}


/**
 * @brief Mengirim data status/sensor/error ke SEMUA client WebSocket yang terhubung.
 */
void notifyWebSocketClients(const char* jsonPayload) {
    ws.textAll(jsonPayload);
}

/**
 * @brief Membuat JSON status terkini untuk dikirim via WebSocket.
 */
void sendCurrentStatusWebSocket() {
    StaticJsonDocument<320> doc;
    doc["type"] = "status"; 
    doc["mode"] = device_mode;
    if (strlen(crop_cycle_id) > 0) {
        doc["crop_cycle_id"] = crop_cycle_id;
    } else {
        doc["crop_cycle_id"] = nullptr;
    }
    doc["timestamp"] = getTimestamp();
    doc["ph_min"] = PH_LOW_LIMIT;
    doc["ph_max"] = PH_HIGH_LIMIT;
    doc["ppm_min"] = PPM_LOW_LIMIT;
    doc["ppm_max"] = PPM_HIGH_LIMIT;

    char output[320];
    serializeJson(doc, output);
    notifyWebSocketClients(output);
}

/**
 * @brief Membuat JSON data sensor terkini untuk dikirim via WebSocket.
 */
void sendSensorDataWebSocket() {
    StaticJsonDocument<200> doc;
    doc["type"] = "data"; 
    doc["ph"] = round(phValue * 100.0) / 100.0;
    doc["ppm"] = round(tdsValue * 100.0) / 100.0;
    doc["temperature"] = round(temperature * 100.0) / 100.0;
    doc["timestamp"] = getTimestamp();

    char output[200];
    serializeJson(doc, output);
    notifyWebSocketClients(output);
}

/**
 * @brief Membuat JSON laporan error sensor untuk dikirim via WebSocket.
 */
void sendSensorErrorWebSocket() {
    StaticJsonDocument<256> errorDoc;
    errorDoc["type"] = "error_report"; 
    errorDoc["message"] = "Sensor failure detected";
    JsonArray sensor_errors = errorDoc.createNestedArray("sensor_errors");
    if (tempErrorState) sensor_errors.add("DS18B20");
    if (phErrorState) sensor_errors.add("pH Sensor");
    if (tdsErrorState) sensor_errors.add("TDS Sensor"); 
    errorDoc["timestamp"] = getTimestamp();

    char errorOutput[256];
    serializeJson(errorDoc, errorOutput);
    notifyWebSocketClients(errorOutput);
}

/**
 * @brief Membuat JSON response untuk perintah yang diterima via WebSocket.
 */
void sendWebSocketResponse(const char* status, const char* message) {
     StaticJsonDocument<128> responseDoc;
     responseDoc["type"] = "response"; 
     responseDoc["status"] = status;
     responseDoc["message"] = message;
     char responseOutput[128];
     serializeJson(responseDoc, responseOutput);
     notifyWebSocketClients(responseOutput); 
}

/**
 * @brief Handler untuk pesan WebSocket yang masuk (perintah dari web).
 */
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *clientWs, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", clientWs->id(), clientWs->remoteIP().toString().c_str());
            sendCurrentStatusWebSocket();
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", clientWs->id());
            break;
        case WS_EVT_DATA: { 
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = 0; 
                Serial.printf("Pesan WebSocket dari #%u: %s\n", clientWs->id(), (char*)data);

                StaticJsonDocument<384> doc; 
                DeserializationError error = deserializeJson(doc, (char*)data);

                if (error) {
                    Serial.print("deserializeJson() gagal dari WS: "); Serial.println(error.c_str());
                    sendWebSocketResponse("error", "Invalid JSON format");
                    return;
                }

                const char* action = doc["action"];

                if (action && strcmp(action, "RESET_WIFI") == 0) {
                    Serial.println("Perintah RESET_WIFI via WebSocket.");
                    sendWebSocketResponse("ok", "WiFi credentials reset. Restarting.");
                    delay(1000); 
                    flagResetWifi = true;
                } else if (action && strcmp(action, "set_wifi") == 0) {
                    if (doc.containsKey("ssid") && doc.containsKey("password")) {
                        String ssid = doc["ssid"]; String pass = doc["password"];
                        Serial.printf("Menyimpan WiFi baru via WebSocket: %s\n", ssid.c_str());
                        
                        preferences.begin(NVS_NAMESPACE, false); 
                        preferences.putString(KEY_SSID, ssid); 
                        preferences.putString(KEY_PASS, pass); 
                        preferences.putBool(KEY_WIFI_ATTEMPT, true); 
                        preferences.end();
                        
                        sendWebSocketResponse("ok", "WiFi credentials saved. Restarting.");
                        delay(1000); 
                        flagRestart = true;
                    } else {
                        sendWebSocketResponse("error", "Missing ssid or password");
                    }
                } else if (action && strcmp(action, "sync_threshold") == 0) {
                    if (doc.containsKey("ph_min") && doc.containsKey("ph_max") && doc.containsKey("ppm_min") && doc.containsKey("ppm_max")) {
                        PH_LOW_LIMIT = doc["ph_min"]; PH_HIGH_LIMIT = doc["ph_max"];
                        PPM_LOW_LIMIT = doc["ppm_min"]; PPM_HIGH_LIMIT = doc["ppm_max"];
                        saveThresholds();
                        sendWebSocketResponse("ok", "Threshold updated and saved");
                        sendCurrentStatusWebSocket(); 
                    } else {
                        sendWebSocketResponse("error", "Incomplete threshold data");
                    }
                } else if (action && strcmp(action, "start_cycle") == 0) {
                    if (doc.containsKey("crop_cycle_id") && doc.containsKey("ph_min") && doc.containsKey("ph_max") && doc.containsKey("ppm_min") && doc.containsKey("ppm_max")) {
                        strncpy(crop_cycle_id, doc["crop_cycle_id"] | "", sizeof(crop_cycle_id) - 1);
                        PH_LOW_LIMIT = doc["ph_min"]; PH_HIGH_LIMIT = doc["ph_max"];
                        PPM_LOW_LIMIT = doc["ppm_min"]; PPM_HIGH_LIMIT = doc["ppm_max"];
                        saveThresholds();
                        strcpy(device_mode, "active"); 
                        cycleStartTime = millis(); 
                        tdsValue = 0; 
                        phValue = 7.0; 
                        tdsBufferReady = false; 
                        Serial.printf("(WS) Cycle %s dimulai. Mode: ACTIVE\n", crop_cycle_id); Serial.printf("Memulai jeda pemanasan sensor %ld detik...\n", sensorWarmUpTime / 1000);
                        
                        sendWebSocketResponse("ok", "Cycle started");
                        sendCurrentStatusWebSocket(); 
                        if(client.connected()) publishHello(); 
                    } else {
                        sendWebSocketResponse("error", "Incomplete cycle data");
                    }
                } else if (action && strcmp(action, "stop_cycle") == 0) {
                    Serial.printf("Cycle %s dihentikan via WebSocket.\n", crop_cycle_id);
                    strcpy(crop_cycle_id, ""); strcpy(device_mode, "idle");
                    digitalWrite(RELAY_CH1_PIN, HIGH); digitalWrite(RELAY_CH2_PIN, HIGH);
                    digitalWrite(RELAY_CH3_PIN, HIGH); digitalWrite(RELAY_CH4_PIN, HIGH);
                    isPompaPhOn = false; isPompaNutrisiOn = false;
                    tdsBufferReady = false; 
                    sendWebSocketResponse("ok", "Cycle stopped");
                    sendCurrentStatusWebSocket();
                    if(client.connected()) publishHello(); 
                } else {
                    sendWebSocketResponse("error", "Unknown action");
                }
            }
            break;
        }
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

/**
 * @brief Mengatur routing server web dan WebSocket.
 */
void setupWebServer() {
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        webUserLoggedIn = false; 
        request->send_P(200, "text/html", loginPageHTML);
    });

    server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("password", true)) { 
            AsyncWebParameter* p = request->getParam("password", true);
            if (p->value().equals(web_password)) {
                webUserLoggedIn = true;
                Serial.println("Web login berhasil.");
                request->redirect("/dashboard"); 
            } else {
                Serial.println("Web login gagal: Password salah.");
                request->redirect("/?error=1"); 
            }
        } else {
            request->redirect("/?error=1"); 
        }
    });

    server.on("/dashboard", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!webUserLoggedIn) {
            request->redirect("/"); 
            return;
        }
        request->send_P(200, "text/html", dashboardPageHTML);
    });

    server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.println("HTTP server internal dimulai.");
}


void setup() {
    Serial.begin(115200);
    Serial.println("\n================================");
    Serial.println("Memulai Perangkat Hidroponik (v7 - TDS Error Fix)...");

    loadSettings();
    setupPins();
    setupWifi(); 
    setupNtp();
    setupMqtt(); 
    setupWebServer(); 

    Serial.println("================================");
    Serial.println("Setup Selesai. Menjalankan program utama.");
    Serial.printf("Akses dashboard web di: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.println("Ketik 'help' di Serial Monitor untuk debug.");
    Serial.println("================================");
}


void publishHello() {
    StaticJsonDocument<200> helloDoc;
    helloDoc["status"] = "online"; helloDoc["mode"] = device_mode;
    if (strlen(crop_cycle_id) > 0) helloDoc["crop_cycle_id"] = crop_cycle_id;
    else helloDoc["crop_cycle_id"] = nullptr;
    helloDoc["timestamp"] = getTimestamp();
    char helloOutput[200];
    serializeJson(helloDoc, helloOutput);
    Serial.print("(MQTT) Mengirim status Online ke: "); Serial.println(hello_topic); Serial.println(helloOutput);
    client.publish(hello_topic, helloOutput);
}

void publishData() {
    StaticJsonDocument<200> doc;
    doc["ph"] = round(phValue * 100.0) / 100.0;
    doc["ppm"] = round(tdsValue * 100.0) / 100.0;
    doc["temperature"] = round(temperature * 100.0) / 100.0;
    doc["timestamp"] = getTimestamp();
    char output[200];
    serializeJson(doc, output);
    Serial.println("================================");
    Serial.print("(MQTT) Mempublikasikan data ke topik: "); Serial.println(mqtt_topic); Serial.println(output);
    if (client.publish(mqtt_topic, output)) Serial.println("Status: Data MQTT berhasil dipublikasikan.");
    else Serial.println("Status: Gagal mempublikasikan data MQTT.");

    sendSensorDataWebSocket();
}

void publishSensorError() {
    StaticJsonDocument<256> errorDoc;
    errorDoc["status"] = "error"; errorDoc["message"] = "Sensor failure detected";
    JsonArray sensor_errors = errorDoc.createNestedArray("sensor_errors");
    if (tempErrorState) sensor_errors.add("DS18B20 (Temperature) disconnected");
    if (phErrorState) sensor_errors.add("pH Sensor value invalid (<= 0)");
    if (tdsErrorState) sensor_errors.add("TDS Sensor value invalid (<= 0)"); 
    errorDoc["timestamp"] = getTimestamp();
    char errorOutput[256];
    serializeJson(errorDoc, errorOutput);
    Serial.println("================================"); Serial.println("SENSOR ERROR DETECTED!");
    Serial.print("(MQTT) Mengirim laporan error ke: "); Serial.println(response_topic); Serial.println(errorOutput);
    client.publish(response_topic, errorOutput);

    sendSensorErrorWebSocket();
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.println("================================");
    Serial.print("(MQTT) Pesan diterima di topik ["); Serial.print(topic); Serial.println("]");
    char message[length + 1]; memcpy(message, payload, length); message[length] = '\0'; Serial.println(message);

    if (strcmp(topic, command_topic) == 0) {
        StaticJsonDocument<384> doc; 
        DeserializationError error = deserializeJson(doc, message);
        StaticJsonDocument<128> responseDoc; char responseOutput[128];

        if (error) {
            Serial.print("deserializeJson() gagal: "); Serial.println(error.c_str());
            responseDoc["status"] = "error"; responseDoc["message"] = "Invalid JSON format";
            serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
            sendWebSocketResponse("error", "Invalid JSON from MQTT"); 
            return;
        }
        
        const char* action = doc["action"];

        if (action && strcmp(action, "RESET_WIFI") == 0) {
             Serial.println("(MQTT) Perintah RESET_WIFI diterima.");
             responseDoc["status"] = "ok"; responseDoc["message"] = "WiFi credentials reset. Restarting.";
             serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
             sendWebSocketResponse("ok", "RESET_WIFI command received via MQTT. Restarting."); 
             delay(1000); 
             flagResetWifi = true;
        }
        else if (action && strcmp(action, "set_wifi") == 0) {
            if (doc.containsKey("ssid") && doc.containsKey("password")) {
                String ssid = doc["ssid"]; String pass = doc["password"];
                Serial.printf("(MQTT) Menyimpan WiFi baru: %s\n", ssid.c_str());

                preferences.begin(NVS_NAMESPACE, false); 
                preferences.putString(KEY_SSID, ssid); 
                preferences.putString(KEY_PASS, pass);
                preferences.putBool(KEY_WIFI_ATTEMPT, true); 
                preferences.end();
                
                responseDoc["status"] = "ok"; responseDoc["message"] = "WiFi credentials saved";
                serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
                sendWebSocketResponse("ok", "WiFi credentials saved via MQTT. Restarting."); 
                delay(1000); 
                flagRestart = true;
            } else {
                Serial.println("Perintah set_wifi tidak lengkap.");
                responseDoc["status"] = "error"; responseDoc["message"] = "Missing ssid or password";
                serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
                sendWebSocketResponse("error", "Incomplete set_wifi command from MQTT");
            }
        }
        else if (action && strcmp(action, "sync_threshold") == 0) {
             if (doc.containsKey("ph_min") && doc.containsKey("ph_max") && doc.containsKey("ppm_min") && doc.containsKey("ppm_max")) {
                PH_LOW_LIMIT = doc["ph_min"]; PH_HIGH_LIMIT = doc["ph_max"];
                PPM_LOW_LIMIT = doc["ppm_min"]; PPM_HIGH_LIMIT = doc["ppm_max"];
                saveThresholds(); 
                responseDoc["status"] = "ok"; responseDoc["message"] = "Threshold updated and saved";
                serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
                sendWebSocketResponse("ok", "Thresholds updated via MQTT");
                sendCurrentStatusWebSocket(); 
          } else {
                Serial.println("Perintah sync_threshold tidak lengkap.");
                responseDoc["status"] = "error"; responseDoc["message"] = "Incomplete threshold data";
                serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
                sendWebSocketResponse("error", "Incomplete sync_threshold command from MQTT");
            }
        }
        else if (action && strcmp(action, "start_cycle") == 0) {
             if (doc.containsKey("crop_cycle_id") && doc.containsKey("ph_min") && doc.containsKey("ph_max") && doc.containsKey("ppm_min") && doc.containsKey("ppm_max")) {
                strncpy(crop_cycle_id, doc["crop_cycle_id"] | "", sizeof(crop_cycle_id) - 1);
                PH_LOW_LIMIT = doc["ph_min"]; PH_HIGH_LIMIT = doc["ph_max"];
                PPM_LOW_LIMIT = doc["ppm_min"]; PPM_HIGH_LIMIT = doc["ppm_max"];
                saveThresholds();
                strcpy(device_mode, "active"); 
                cycleStartTime = millis(); 
                tdsValue = 0; 
                phValue = 7.0; 
                tdsBufferReady = false; 
                Serial.printf("(MQTT) Cycle %s dimulai. Mode: ACTIVE\n", crop_cycle_id); Serial.printf("Memulai jeda pemanasan sensor %ld detik...\n", sensorWarmUpTime / 1000);
                publishHello(); 
                sendCurrentStatusWebSocket(); 
                responseDoc["status"] = "ok"; responseDoc["message"] = "Cycle started";
                serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
            } else {
                Serial.println("Perintah start_cycle tidak lengkap.");
                responseDoc["status"] = "error"; responseDoc["message"] = "Incomplete cycle data";
                serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
                sendWebSocketResponse("error", "Incomplete start_cycle command from MQTT");
            }
        }
        else if (action && strcmp(action, "stop_cycle") == 0) {
            Serial.printf("(MQTT) Cycle %s dihentikan.\n", crop_cycle_id);
            strcpy(crop_cycle_id, ""); strcpy(device_mode, "idle");
            digitalWrite(RELAY_CH1_PIN, HIGH); digitalWrite(RELAY_CH2_PIN, HIGH); digitalWrite(RELAY_CH3_PIN, HIGH); digitalWrite(RELAY_CH4_PIN, HIGH);
            isPompaPhOn = false; isPompaNutrisiOn = false;
            tdsBufferReady = false; 
            publishHello(); 
            sendCurrentStatusWebSocket(); 
            responseDoc["status"] = "ok"; responseDoc["message"] = "Cycle stopped";
            serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
        }
        else {
            Serial.print("Perintah MQTT tidak dikenal: "); if(action) Serial.println(action); else Serial.println("null");
            responseDoc["status"] = "error"; responseDoc["message"] = "Unknown action";
            serializeJson(responseDoc, responseOutput); client.publish(response_topic, responseOutput);
             sendWebSocketResponse("error", "Unknown command from MQTT");
        }
    }
}

void handleMqttConnection() {
    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > mqttReconnectInterval) {
            lastMqttReconnectAttempt = now;
            Serial.print("Mencoba koneksi MQTT...");
            if (client.connect(device_id, mqtt_user, mqtt_pass)) {
                Serial.println("terhubung!"); publishHello(); client.subscribe(command_topic); Serial.print("Subscribe MQTT ke: "); Serial.println(command_topic);
            } else {
                Serial.print("gagal, rc="); Serial.print(client.state()); Serial.println(" coba lagi...");
            }
        }
    }
}

float getTemperature(){
    sensors.requestTemperatures(); 
    float tempC = sensors.getTempCByIndex(0);
    return (tempC == DEVICE_DISCONNECTED_C) ? DEVICE_DISCONNECTED_C : tempC;
}

/**
 * @brief [MODIFIKASI v7] Mengatur tdsBufferReady menjadi true setelah perhitungan pertama.
 */
void tdsCalibrate(float currentTemp) { 
    static unsigned long analogSampleTimepoint = millis();
    if(millis()-analogSampleTimepoint > 40U) { 
        analogSampleTimepoint = millis();
        analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin); 
        analogBufferIndex = (analogBufferIndex + 1) % SCOUNT;
    } 

    static unsigned long printTimepoint = millis();
    if(millis()-printTimepoint > 800U) { 
        printTimepoint = millis();
        for(copyIndex=0;copyIndex<SCOUNT;copyIndex++) {
             analogBufferTemp[copyIndex]= analogBuffer[copyIndex];
             if (analogBuffer[copyIndex] == 0 && !tdsBufferReady) {
             }
        }

        int rawMedian = getMedianNum(analogBufferTemp,SCOUNT);
        averageVoltage = rawMedian * (float)VREF / 4096.0; 
        float compensationCoefficient=1.0+0.02*(currentTemp-25.0);
        float compensationVolatge=averageVoltage/compensationCoefficient; 
        tdsValue=(133.42*compensationVolatge*compensationVolatge*compensationVolatge - 255.86*compensationVolatge*compensationVolatge + 857.39*compensationVolatge)*0.5; 
        tdsValue = tdsValue * FAKTOR_KOREKSI;

        if (!tdsBufferReady && rawMedian > 0) { 
            tdsBufferReady = true;
            Serial.println("Buffer TDS siap, pengecekan error TDS diaktifkan.");
        }
    }
}

void pHCalibrate(){
    static unsigned long timepoint = millis();
    if(millis()-timepoint>1000U){     
        timepoint = millis();
        voltage = analogRead(PH_PIN)/4095.0*3300;
        if (voltage >= neutralVoltage) { 
            float slope = (7.0 - 4.0) / (neutralVoltage - acidVoltage);
            phValue = 4.0 + slope * (voltage - acidVoltage);
        } else { 
            float slope = (10.0 - 7.0) / (alkalineVoltage - neutralVoltage);
            phValue = 7.0 + slope * (voltage - neutralVoltage);
        }
    }
}

void cekKondisiPompa() {
    if (strcmp(device_mode, "active") != 0) return;
    unsigned long now = millis();
    if (isPompaPhOn || isPompaNutrisiOn) return; 
    if (now - lastDoseTime < mixingInterval) return; 

    bool canCheckTDS = tdsBufferReady; 

    if (phValue < PH_LOW_LIMIT && phValue > 0) { 
        Serial.println("AKSI POMPA: pH rendah. Menyalakan pH Up...");
        digitalWrite(RELAY_CH1_PIN, LOW); digitalWrite(RELAY_CH2_PIN, HIGH);
        isPompaPhOn = true; pompaPhStartTime = now; lastDoseTime = now;
    } 
    else if (phValue > PH_HIGH_LIMIT) {
        Serial.println("AKSI POMPA: pH tinggi. Menyalakan pH Down...");
        digitalWrite(RELAY_CH1_PIN, HIGH); digitalWrite(RELAY_CH2_PIN, LOW);
        isPompaPhOn = true; pompaPhStartTime = now; lastDoseTime = now;
    } 
    else if (canCheckTDS && tdsValue < PPM_LOW_LIMIT && tdsValue > 0) { 
        Serial.println("AKSI POMPA: PPM rendah. Menambah Nutrisi A+B...");
        digitalWrite(RELAY_CH3_PIN, LOW); digitalWrite(RELAY_CH4_PIN, LOW);
        isPompaNutrisiOn = true; pompaNutrisiStartTime = now; lastDoseTime = now;
    }
}

void loopPompa() {
    if (strcmp(device_mode, "active") != 0) return;
    unsigned long now = millis();
    if (isPompaPhOn && (now - pompaPhStartTime >= DURASI_PULSE_POMPA)) {
        Serial.println("AKSI POMPA: Dosis pH selesai.");
        digitalWrite(RELAY_CH1_PIN, HIGH); digitalWrite(RELAY_CH2_PIN, HIGH); isPompaPhOn = false;
    }
    if (isPompaNutrisiOn && (now - pompaNutrisiStartTime >= DURASI_PULSE_POMPA)) {
        Serial.println("AKSI POMPA: Dosis Nutrisi selesai.");
        digitalWrite(RELAY_CH3_PIN, HIGH); digitalWrite(RELAY_CH4_PIN, HIGH); isPompaNutrisiOn = false;
    }
    if (!isPompaPhOn && !isPompaNutrisiOn) { 
        cekKondisiPompa(); 
    }
}

void checkScheduledRestart() {
    unsigned long now = millis();
    if (now - lastTimeCheck > dailyCheckInterval) { 
        lastTimeCheck = now;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && !sudahRestartHariIni) {
                Serial.println("Waktu 00:00. Restart terjadwal...");
                char offlineMsg[50]; sprintf(offlineMsg, "{\"status\":\"restarting_daily\"}");
                if(client.connected()) client.publish(hello_topic, offlineMsg); 
                sendWebSocketResponse("info", "Scheduled daily restart initiated."); 
                delay(1000); 
                flagRestart = true; 
            }
            if (timeinfo.tm_hour == 1) {
                if (sudahRestartHariIni) Serial.println("Flag restart harian di-reset.");
                sudahRestartHariIni = false;
            }
        } else {
            Serial.println("Gagal cek waktu lokal untuk penjadwalan restart.");
        }
    }
}

float readPhVoltage() {
    long totalVoltage = 0;
    Serial.print("Membaca voltase pH (100 sampel)... ");
    for(int i = 0; i < 100; i++) { totalVoltage += analogRead(PH_PIN); delay(2); }
    Serial.println("Selesai.");
    float avgRaw = (float)totalVoltage / 100.0;
    return (avgRaw / 4095.0) * 3300.0;
}

float readRawTDS() {
    Serial.print("Membaca voltase TDS (30 sampel)... ");
    for(int i=0; i < SCOUNT; i++) { 
        analogBuffer[i] = analogRead(TdsSensorPin); 
        delay(5); 
    }
    Serial.println("Selesai mengisi buffer.");
    
    for(int idx=0; idx<SCOUNT; idx++) {
        analogBufferTemp[idx]= analogBuffer[idx];
    }
    
    float temp = getTemperature();
    if (temp == DEVICE_DISCONNECTED_C) { 
        Serial.println("Peringatan: Suhu tidak terdeteksi, pakai 25.0 C"); 
        temp = 25.0; 
    }
    
    int rawMedian = getMedianNum(analogBufferTemp, SCOUNT);
    Serial.print("Nilai median mentah: "); Serial.println(rawMedian);
    float avgVolt = rawMedian * (float)VREF / 4096.0; 
    Serial.print("Voltase rata-rata (setelah median): "); Serial.println(avgVolt, 3);
    
    float compCoeff=1.0+0.02*(temp-25.0); 
    float compVolt=avgVolt/compCoeff; 
    Serial.print("Voltase terkompensasi suhu: "); Serial.println(compVolt, 3);
    
    float calculatedTds = (133.42*compVolt*compVolt*compVolt - 255.86*compVolt*compVolt + 857.39*compVolt)*0.5; 
    return calculatedTds;
}


void printStatus() {
    Serial.println("--- Nilai Kalibrasi Saat Ini (RAM) ---");
    Serial.print("  acidVoltage = "); Serial.print(acidVoltage, 2); Serial.println(";");
    Serial.print("  neutralVoltage = "); Serial.print(neutralVoltage, 2); Serial.println(";");
    Serial.print("  alkalineVoltage = "); Serial.print(alkalineVoltage, 2); Serial.println(";");
    Serial.print("  FAKTOR_KOREKSI = "); Serial.print(FAKTOR_KOREKSI, 4); Serial.println(";");
    Serial.println("--- Batas Threshold Saat Ini (RAM) ---");
    Serial.print("  pH: "); Serial.print(PH_LOW_LIMIT, 1); Serial.print(" - "); Serial.print(PH_HIGH_LIMIT, 1); Serial.println();
    Serial.print("  PPM: "); Serial.print(PPM_LOW_LIMIT, 0); Serial.print(" - "); Serial.print(PPM_HIGH_LIMIT, 0); Serial.println();
    Serial.println("-----------------------------------");
}

void printCurrentReadings() {
    float currentTemperature = getTemperature();
    if (currentTemperature == DEVICE_DISCONNECTED_C) { temperature = 25.0; Serial.println("PERINGATAN: Sensor Suhu Gagal!"); } 
    else { temperature = currentTemperature; }
    pHCalibrate(); 
    tdsCalibrate(temperature); 
    Serial.print("Bacaan Sensor -> ");
    Serial.print("Suhu: "); Serial.print(temperature, 2); Serial.print(" *C | ");
    Serial.print("pH: "); Serial.print(phValue, 2); Serial.print(" | ");
    if (tdsBufferReady) { 
      Serial.print("TDS: "); Serial.print(tdsValue, 0); Serial.println(" ppm");
    } else {
      Serial.println("TDS: (Menunggu buffer siap...)");
    }
    Serial.print(" (Debug pH Volt: "); Serial.print(voltage, 1); Serial.print("mV)"); Serial.println();
}

void printHelp() {
    Serial.println("--- Daftar Perintah Serial ---");
    Serial.println("  help            : Tampilkan pesan ini");
    Serial.println("  baca            : Baca & tampilkan sensor");
    Serial.println("  status          : Tampilkan kalibrasi & threshold");
    Serial.println("  ph4             : Kalibrasi pH 4.0 (Akan DISIMPAN)");
    Serial.println("  ph7             : Kalibrasi pH 7.0 (Akan DISIMPAN)");
    Serial.println("  ph10            : Kalibrasi pH 10.0 (Akan DISIMPAN)");
    Serial.println("  tdsXXXX         : Kalibrasi TDS (mis: tds1382) (Akan DISIMPAN)");
    Serial.println("  testrelay       : Uji siklus semua relay");
    Serial.println("  relay[1-4]on    : Nyalakan relay (mis: relay1on)");
    Serial.println("  relay[1-4]off   : Matikan relay (mis: relay1off)");
    Serial.println("  resetwifi       : Hapus WiFi & restart");
    Serial.println("  restart         : Restart paksa perangkat");
    Serial.println("------------------------------");
}

void testRelay(){
    Serial.println("Tes Relay: Semua ON (LOW)...");
    digitalWrite(RELAY_CH1_PIN, LOW); digitalWrite(RELAY_CH2_PIN, LOW); digitalWrite(RELAY_CH3_PIN, LOW); digitalWrite(RELAY_CH4_PIN, LOW);
    delay(500);
    Serial.println("Tes Relay: Semua OFF (HIGH)...");
    digitalWrite(RELAY_CH1_PIN, HIGH); digitalWrite(RELAY_CH2_PIN, HIGH); digitalWrite(RELAY_CH3_PIN, HIGH); digitalWrite(RELAY_CH4_PIN, HIGH);
    delay(200);
}

void handleSerialCommand() {
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n'); command.trim();
        Serial.print("Perintah Serial: "); Serial.println(command);
        
        if (command.equals("help")) printHelp();
        else if (command.equals("baca")) printCurrentReadings();
        else if (command.equals("status")) printStatus();
        else if (command.equals("ph4")) { Serial.print("Kalibrasi pH 4.0... "); acidVoltage = readPhVoltage(); savePhCalibration(); Serial.print("-> Acid Volt: "); Serial.println(acidVoltage); printStatus(); }
        else if (command.equals("ph7")) { Serial.print("Kalibrasi pH 7.0... "); neutralVoltage = readPhVoltage(); savePhCalibration(); Serial.print("-> Neutral Volt: "); Serial.println(neutralVoltage); printStatus(); }
        else if (command.equals("ph10")) { Serial.print("Kalibrasi pH 10.0... "); alkalineVoltage = readPhVoltage(); savePhCalibration(); Serial.print("-> Alkaline Volt: "); Serial.println(alkalineVoltage); printStatus(); }
        else if (command.startsWith("tds")) {
            float targetTDS = command.substring(3).toFloat();
            if (targetTDS > 0) {
                Serial.printf("Kalibrasi TDS ke %0.f ppm...\n", targetTDS);
                float rawTDS = readRawTDS(); 
                Serial.print("Bacaan mentah (sebelum koreksi): "); Serial.print(rawTDS, 2); Serial.println(" ppm");
                if (rawTDS > 0) { 
                    FAKTOR_KOREKSI = targetTDS / rawTDS; 
                    saveTdsCalibration(); 
                    Serial.print("-> Faktor Koreksi baru: "); Serial.println(FAKTOR_KOREKSI, 4); 
                    printStatus(); 
                    tdsValue = rawTDS * FAKTOR_KOREKSI; 
                    Serial.print("-> Nilai TDS terkoreksi sekarang: "); Serial.println(tdsValue, 0);
                    tdsBufferReady = true; 
                } 
                else { Serial.println("Gagal! TDS mentah 0 atau negatif."); }
            } else { Serial.println("Format salah. Cth: tds1000"); }
        } else if (command.equals("testrelay")) testRelay();
        else if (command.equals("resetwifi")) { Serial.println("Hapus WiFi & restart..."); flagResetWifi = true; } 
        else if (command.equals("restart")) { Serial.println("Restart paksa..."); flagRestart = true; } 
        else if (command.equals("relay1on")) digitalWrite(RELAY_CH1_PIN, LOW); else if (command.equals("relay1off")) digitalWrite(RELAY_CH1_PIN, HIGH);
        else if (command.equals("relay2on")) digitalWrite(RELAY_CH2_PIN, LOW); else if (command.equals("relay2off")) digitalWrite(RELAY_CH2_PIN, HIGH);
        else if (command.equals("relay3on")) digitalWrite(RELAY_CH3_PIN, LOW); else if (command.equals("relay3off")) digitalWrite(RELAY_CH3_PIN, HIGH);
        else if (command.equals("relay4on")) digitalWrite(RELAY_CH4_PIN, LOW); else if (command.equals("relay4off")) digitalWrite(RELAY_CH4_PIN, HIGH);
        else Serial.println("Perintah tidak dikenal. Ketik 'help'.");
    }
}

void handleWifiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        if (firstWifiDisconnectTime == 0) { firstWifiDisconnectTime = millis(); Serial.println("\nWiFi terputus, mulai timer restart..."); }
        Serial.print(".");
        if (millis() > 60000) { 
            if (millis() - firstWifiDisconnectTime > wifiRestartTimeout) { 
                Serial.println("WiFi gagal konek > 5 menit. Restart."); 
                delay(1000); 
                ESP.restart(); 
  t         }
        }
        return; 
    }
    if (firstWifiDisconnectTime > 0) { Serial.println("WiFi terhubung kembali!"); firstWifiDisconnectTime = 0; }
}

void loop() {
    if (flagResetWifi) {
        Serial.println("Flag 'flagResetWifi' terdeteksi. Menghapus kredensial WiFi dan restart...");
        preferences.begin(NVS_NAMESPACE, false);
        preferences.remove(KEY_SSID);
        preferences.remove(KEY_PASS);
        preferences.remove(KEY_WIFI_ATTEMPT); 
        preferences.end();
        delay(1000); 
        ESP.restart();
    }
    if (flagRestart) {
        Serial.println("Flag 'flagRestart' terdeteksi. Melakukan restart...");
        delay(1000); 
        ESP.restart();
    }

    unsigned long now = millis();

    handleWifiConnection();
    if (WiFi.status() != WL_CONNECTED) return; 

    handleMqttConnection(); 
    if (client.connected()) client.loop(); 

    ws.cleanupClients(); 
    handleSerialCommand(); 
    checkScheduledRestart(); 

    if (strcmp(device_mode, "idle") == 0) {
        if (now - lastHelloTime > helloInterval) {
            lastHelloTime = now;
            if (client.connected()) publishHello(); 
            sendCurrentStatusWebSocket(); 
        }
    } 
    else if (strcmp(device_mode, "active") == 0) {
        float currentTemperature = getTemperature();
        tempErrorState = (currentTemperature == DEVICE_DISCONNECTED_C);
        if (!tempErrorState) {
            temperature = currentTemperature;
        } else {
             temperature = 25.0; 
             Serial.println("PERINGATAN: Sensor Suhu Gagal! Menggunakan 25.0 C untuk kalkulasi TDS.");
        }
        
        pHCalibrate(); phErrorState = (phValue <= 0);
        tdsCalibrate(temperature); 
        
        tdsErrorState = tdsBufferReady && (tdsValue <= 0);s  
        
        if (cycleStartTime != 0 && now - cycleStartTime < sensorWarmUpTime) {
            Serial.print("Sensor warming up... ");
            unsigned long remainingWarmUp = sensorWarmUpTime - (now - cycleStartTime);
            Serial.print(remainingWarmUp / 1000); Serial.println(" detik tersisa.");
            if (tdsBufferReady) {
                tdsBufferReady = false;s  
                Serial.println("Reset flag tdsBufferReady selama pemanasan.");
            }
            return; 
        } else if (cycleStartTime != 0) {
            Serial.println("Jeda pemanasan sensor umum selesai."); 
            cycleStartTime = 0; 
        }

        loopPompa();

        bool anySensorError = tempErrorState || phErrorState || tdsErrorState;s  
        
        if (!anySensorError) { 
            if (tdsBufferReady && (now - lastPublishTime > publishInterval)) {
                lastPublishTime = now;
                if (client.connected()) publishData(); 
                else sendSensorDataWebSocket(); 
            } else if (!tdsBufferReady) {
                 Serial.println("Menunggu buffer TDS siap sebelum publish data...");
            }
            lastErrorPublishTime = 0; 
        } else { 
            if (now - lastErrorPublishTime > errorPublishInterval || lastErrorPublishTime == 0) {
                lastErrorPublishTime = now;
                if (client.connected()) publishSensorError(); 
                else sendSensorErrorWebSocket(); 
            }
            lastPublishTime = now; 
        }
    }
}

