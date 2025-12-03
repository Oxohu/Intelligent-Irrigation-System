// ===== INTELLIGENT IRRIGATION SYSTEM WITH WEB SERVER =====
// ESP32 with WiFi Web Server, Manual Override, ML Auto, and Data Visualization

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ===== WIFI CREDENTIALS =====
const char* ssid = "freewifi";
const char* password = "@sunny123";

// ===== PIN DEFINITIONS (ESP32) =====
#define LED_GREEN     2      // Built-in LED
#define BUZZER_PIN    4      // Buzzer
#define RELAY_PIN     5      // Relay control pin
#define SOIL_PIN      36     // Soil moisture sensor
#define WATER_PIN     39     // Water level sensor

// ===== SENSOR THRESHOLDS ===== 
int SOIL_DRY_THRESHOLD   = 4095;   // Soil is dry
int SOIL_WET_THRESHOLD   = 4094;   // Soil is wet
int WATER_GOOD_THRESHOLD = 2000;   // Water level good
int WATER_LOW_THRESHOLD  = 1999;   // Water level low

// ===== TIMING SETTINGS =====
const unsigned long PUMP_MAX_ON_TIME    = 60000;  // 60 seconds max
const unsigned long SENSOR_CHECK_TIME   = 10000;   // 10 seconds for demo

// ===== SYSTEM VARIABLES =====
bool pumpIsRunning = false;
bool soilIsDry = false;        
bool waterLevelGood = false;   
bool manualMode = false;       // Manual override
bool mlAutoMode = false;       // ML Auto mode
bool manualPumpState = false;  // Manual pump state
bool autoMode = false;         // Auto mode
unsigned long pumpDuration = 60000;  // Dynamic pump duration (ms), default 60s
unsigned long pumpStartTime = 0;
unsigned long lastSensorCheck = 0;
unsigned long lastMLCheck = 0;  // Track last ML decision
unsigned long lastWaterTime = 0;  // Track last watering
int previousSoil = 0;  // For drying_rate

// ===== WEB SERVER =====
WebServer server(80);

// ===== SENSOR READING WITH SMOOTHING =====
int readSensorSmooth(int pin) {
  long total = 0;
  for (int i = 0; i < 5; i++) {
    total += analogRead(pin);
    delay(10);
  }
  return total / 5;
}

// ===== PUMP CONTROL =====
void startPump(unsigned long duration) {
  if (!pumpIsRunning) {
    digitalWrite(RELAY_PIN, HIGH);
    pumpIsRunning = true;
    pumpStartTime = millis();
    lastWaterTime = pumpStartTime;  // Update last watering time
    pumpDuration = duration;  // Set specific duration for this cycle
    Serial.println("PUMP STARTED for " + String(duration / 1000) + " seconds");
  }
}

void stopPump() {
  if (pumpIsRunning) {
    digitalWrite(RELAY_PIN, LOW);
    pumpIsRunning = false;
    Serial.println("PUMP STOPPED");
  }
}

// ===== BUZZER CONTROL =====
void updateBuzzer(int waterReading) {
  digitalWrite(BUZZER_PIN, waterReading < 2000); // Buzz when water < 2000
}

// ===== LED STATUS DISPLAY =====
void updateLEDs(int waterReading) {
  digitalWrite(LED_GREEN, waterReading >= 2000); // Green LED on when water >= 2000
}

// ===== HISTORICAL DATA STORAGE =====
const int MAX_HISTORY = 100;
struct SensorReading {
  unsigned long timestamp;
  int soil;
  int water;
};
SensorReading history[MAX_HISTORY];
int historyCount = 0;
int historyIndex = 0;

void addToHistory(int soil, int water) {
  history[historyIndex].timestamp = millis();
  history[historyIndex].soil = soil;
  history[historyIndex].water = water;
  historyIndex = (historyIndex + 1) % MAX_HISTORY;
  if (historyCount < MAX_HISTORY) historyCount++;
}

// ===== WEB PAGE HTML =====
const char webPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Intelligent Irrigation System</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; flex-direction: column; background-color: #f0f8ff; }
        .sidebar { width: 100%; background-color: #f0f0f0; padding: 10px; position: relative; }
        .menu-toggle { font-size: 1.5em; background: none; border: none; cursor: pointer; padding: 5px; }
        .menu-toggle:hover { color: #666; }
        .sidebar ul { list-style-type: none; padding: 0; display: none; margin: 0; }
        .sidebar.active ul { display: block; }
        .sidebar li { margin: 5px 0; }
        .sidebar a { text-decoration: none; color: #333; display: block; padding: 8px; background: #ddd; border-radius: 5px; font-size: 0.9em; }
        .sidebar a:hover { background: #ccc; }
        .content { padding: 10px; width: 100%; }
        .header { text-align: center; color: #2e8b57; margin-bottom: 15px; background: white; padding: 10px; border-radius: 5px; box-shadow: 0 2px 3px rgba(0,0,0,0.1); }
        .control-panel { background: white; padding: 15px; border-radius: 5px; margin-bottom: 15px; box-shadow: 0 2px 3px rgba(0,0,0,0.1); text-align: center; }
        button { padding: 10px 15px; margin: 5px; font-size: 1em; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; min-width: 100px; }
        .btn-on { background-color: #4CAF50; color: white; }
        .btn-off { background-color: #f44336; color: white; }
        .btn-auto { background-color: #008CBA; color: white; }
        .btn-ml-auto { background-color: #6A5ACD; color: white; }
        button:hover { opacity: 0.8; transform: translateY(-1px); }
        .status-box { background: #e8f5e8; padding: 10px; border-radius: 5px; margin: 10px 0; border-left: 3px solid #4CAF50; }
        .charts-container { background: white; padding: 15px; border-radius: 5px; box-shadow: 0 2px 3px rgba(0,0,0,0.1); margin-bottom: 15px; }
        .chart-row { display: flex; flex-direction: column; gap: 15px; }
        .chart-box { background: #f9f9f9; padding: 10px; border-radius: 5px; border: 1px solid #ddd; }
        #status { font-weight: bold; color: #2e8b57; font-size: 0.9em; line-height: 1.3; }
        .sensor-reading { font-size: 1.2em; font-weight: bold; margin: 5px 0; }
        .soil-reading { color: #8b4513; }
        .water-reading { color: #1e90ff; }
        #profile, #soil-data, #water-data { display: none; background: #fff; padding: 10px; border: 1px solid #ddd; border-radius: 5px; margin-top: 10px; }
        table { width: 100%; border-collapse: collapse; font-size: 0.9em; }
        th, td { border: 1px solid #ddd; padding: 5px; text-align: left; }
        th { background-color: #f2f2f2; }
        @media (min-width: 768px) {
            .sidebar { width: 250px; position: fixed; height: 100vh; }
            .sidebar ul { display: block; }
            .menu-toggle { display: none; }
            .content { margin-left: 250px; width: calc(100% - 250px); }
            .chart-row { flex-direction: row; }
            .chart-box { flex: 1; }
            button { font-size: 1.2em; padding: 15px 30px; }
            .status-box { padding: 20px; }
            .charts-container { padding: 30px; }
            #status { font-size: 1em; }
            .sensor-reading { font-size: 1.5em; }
            table { font-size: 1em; }
        }
    </style>
</head>
<body>
    <div class="sidebar">
        <button class="menu-toggle" onclick="toggleMenu()">☰</button>
        <ul>
            <li><a href="#" onclick="showSection('profile')">Farmer Profile</a></li>
            <li><a href="#" onclick="showSection('soil-data')">Soil Moisture Data</a></li>
            <li><a href="#" onclick="showSection('water-data')">Water Level Data</a></li>
            <li><a href="/csv">Download CSV</a></li>
        </ul>
    </div>
    <div class="content">
        <div class="header">
            <h1>🌱 Intelligent Irrigation System</h1>
            <p>Remote Control & Real-time Monitoring</p>
        </div>
        
        <div class="control-panel">
            <h2>Control Panel</h2>
            <button class="btn-on" onclick="sendCommand('on')">🟢 PUMP ON</button>
            <button class="btn-off" onclick="sendCommand('off')">🔴 PUMP OFF</button>
            <button class="btn-auto" onclick="sendCommand('auto')">🤖 AUTO MODE</button>
            <button class="btn-ml-auto" onclick="sendCommand('ml_auto')">🧠 ML AUTO MODE</button>
            <div class="status-box">
                <div id="status">🔄 Loading system status...</div>
            </div>
        </div>
        
        <div class="charts-container">
            <h2>📊 Live Sensor Data</h2>
            <div class="chart-row">
                <div class="chart-box">
                    <h3>🌱 Soil Moisture</h3>
                    <div class="sensor-reading soil-reading" id="soilValue">---</div>
                    <div>Status: <span id="soilStatus">---</span></div>
                    <canvas id="soilChart" width="300" height="150"></canvas>
                </div>
                <div class="chart-box">
                    <h3>💧 Water Level</h3>
                    <div class="sensor-reading water-reading" id="waterValue">---</div>
                    <div>Status: <span id="waterStatus">---</span></div>
                    <canvas id="waterChart" width="300" height="150"></canvas>
                </div>
            </div>c:\Users\HomePC\Documents\Arduino\libraries
        </div>
        
        <div id="profile">
            <h3>Farmer Profile</h3>
            <p>Name: DR. Talhah Foluronsho</p>
            <p>Farmland Size: 58.42cm By 81.28cm</p>
            <p>Crops: Rice, Maize</p>
            <p>Soil Type: Sandy Loam</p>
            <p>Location: Niger State, Nigeria</p>
        </div>
        
        <div id="soil-data">
            <h3>Soil Moisture Historical Data</h3>
            <table id="soil-table">
                <thead><tr><th>Time (ms)</th><th>Reading</th><th>Status</th></tr></thead>
                <tbody></tbody>
            </table>
        </div>
        
        <div id="water-data">
            <h3>Water Level Historical Data</h3>
            <table id="water-table">
                <thead><tr><th>Time (ms)</th><th>Reading</th><th>Status</th></tr></thead>
                <tbody></tbody>
            </table>
        </div>
    </div>

    <script>
        let soilData = [];
        let waterData = [];
        const maxDataPoints = 20;
        const SOIL_DRY = 4095;
        const SOIL_WET = 4094;
        const WATER_GOOD = 2000;
        const WATER_LOW = 1999;
        const maxVal = 4095;

        function drawChart(canvasId, data, color, invert) {
            const canvas = document.getElementById(canvasId);
            const ctx = canvas.getContext('2d');
            const width = canvas.width;
            const height = canvas.height;
            ctx.clearRect(0, 0, width, height);
            if (data.length < 2) return;
            ctx.strokeStyle = '#e0e0e0';
            ctx.lineWidth = 1;
            for (let i = 0; i < 5; i++) {
                const y = (height / 4) * i;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(width, y);
                ctx.stroke();
            }
            ctx.strokeStyle = color;
            ctx.lineWidth = 2;
            ctx.beginPath();
            const step = width / (maxDataPoints - 1);
            for (let i = 0; i < data.length; i++) {
                let val = data[i];
                if (invert) val = maxVal - val;
                const y = height - (val / maxVal * height);
                const x = i * step;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();
            ctx.fillStyle = color;
            for (let i = 0; i < data.length; i++) {
                let val = data[i];
                if (invert) val = maxVal - val;
                const y = height - (val / maxVal * height);
                const x = i * step;
                ctx.beginPath();
                ctx.arc(x, y, 3, 0, 2 * Math.PI);
                ctx.fill();
            }
        }

        function toggleMenu() {
            document.querySelector('.sidebar').classList.toggle('active');
        }

        function showSection(section) {
            document.querySelectorAll('#profile, #soil-data, #water-data').forEach(el => el.style.display = 'none');
            document.getElementById(section).style.display = 'block';
            if (section === 'soil-data' || section === 'water-data') fetchHistory();
        }

        function sendCommand(cmd) {
            fetch('/control', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({command: cmd})
            }).then(response => { if (response.ok) updateStatus(); });
        }

        function updateStatus() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('status').innerHTML = `
                        Mode: ${data.mode}<br>
                        Pump: ${data.pump}<br>
                        Soil: ${data.soil} (${data.soilStatus})<br>
                        Water: ${data.water} (${data.waterStatus})
                    `;
                    document.getElementById('soilValue').textContent = data.soil;
                    document.getElementById('soilStatus').textContent = data.soilStatus;
                    document.getElementById('waterValue').textContent = data.water;
                    document.getElementById('waterStatus').textContent = data.waterStatus;
                    if (soilData.length >= maxDataPoints) {
                        soilData.shift();
                        waterData.shift();
                    }
                    soilData.push(parseInt(data.soil));
                    waterData.push(parseInt(data.water));
                    drawChart('soilChart', soilData, '#8b4513', true);
                    drawChart('waterChart', waterData, '#1e90ff', false);
                });
        }

        function fetchHistory() {
            fetch('/history')
                .then(response => response.json())
                .then(data => {
                    const soilTbody = document.querySelector('#soil-table tbody');
                    const waterTbody = document.querySelector('#water-table tbody');
                    soilTbody.innerHTML = '';
                    waterTbody.innerHTML = '';
                    data.forEach(reading => {
                        const soilStatus = reading.soil == 4095 ? 'DRY' : 'WET';
                        const waterStatus = reading.water >= 2000 ? 'GOOD' : 'LOW';
                        soilTbody.innerHTML += `<tr><td>${reading.timestamp}</td><td>${reading.soil}</td><td>${soilStatus}</td></tr>`;
                        waterTbody.innerHTML += `<tr><td>${reading.timestamp}</td><td>${reading.water}</td><td>${waterStatus}</td></tr>`;
                    });
                });
        }

        setInterval(updateStatus, 2000);
        updateStatus();
    </script>
</body>
</html>
)rawliteral";

// ===== WEB SERVER HANDLERS =====
void handleRoot() {
  server.send_P(200, "text/html", webPage);
}

void handleControl() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(200);
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (!error) {
      String command = doc["command"];
      
      if (command == "on") {
        manualMode = true;
        mlAutoMode = false;
        autoMode = false;
        manualPumpState = true;
        startPump(60000);  // 60 seconds max
        Serial.println("Manual ON command received");
      } 
      else if (command == "off") {
        manualMode = true;
        mlAutoMode = false;
        autoMode = false;
        manualPumpState = false;
        stopPump();
        Serial.println("Manual OFF command received");
      } 
      else if (command == "auto") {
        manualMode = false;
        mlAutoMode = false;
        autoMode = true;
        manualPumpState = false;
        stopPump();
        Serial.println("AUTO mode activated");
      }
      else if (command == "ml_auto") {
        manualMode = false;
        mlAutoMode = true;
        autoMode = true;
        manualPumpState = false;
        stopPump();
        Serial.println("ML AUTO mode activated");
      }
      
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  } else {
    server.send(400, "text/plain", "No Data");
  }
}

void handleData() {
  int soilReading = readSensorSmooth(SOIL_PIN);
  int waterReading = readSensorSmooth(WATER_PIN);
  
  DynamicJsonDocument doc(400);
  doc["mode"] = manualMode ? "MANUAL" : (mlAutoMode ? "ML AUTO" : (autoMode ? "AUTO" : "OFF"));
  doc["pump"] = pumpIsRunning ? "ON" : "OFF";
  doc["soil"] = soilReading;
  doc["water"] = waterReading;
  doc["soilStatus"] = soilIsDry ? "DRY" : "WET";
  doc["waterStatus"] = waterLevelGood ? "GOOD" : "LOW";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleHistory() {
  DynamicJsonDocument doc(1024 * 4);
  JsonArray arr = doc.to<JsonArray>();
  
  int start = (historyCount == MAX_HISTORY) ? historyIndex : 0;
  for (int i = 0; i < historyCount; i++) {
    int idx = (start + i) % MAX_HISTORY;
    JsonObject obj = arr.createNestedObject();
    obj["timestamp"] = history[idx].timestamp;
    obj["soil"] = history[idx].soil;
    obj["water"] = history[idx].water;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleCSV() {
  String csv = "Timestamp,Soil,Water\n";
  int start = (historyCount == MAX_HISTORY) ? historyIndex : 0;
  for (int i = 0; i < historyCount; i++) {
    int idx = (start + i) % MAX_HISTORY;
    csv += String(history[idx].timestamp) + "," + String(history[idx].soil) + "," + String(history[idx].water) + "\n";
  }
  server.send(200, "text/csv", csv);
  server.setContentLength(csv.length());
  server.sendHeader("Content-Disposition", "attachment; filename=historical_data.csv");
}

// ===== SETUP =====
void setup() {
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("==========================================");
  Serial.println("INTELLIGENT IRRIGATION - ESP32 WEB SERVER");
  Serial.println("==========================================");
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Open this URL in your browser: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connection failed! Check credentials.");
    Serial.println("Running in offline mode...");
    }
  
  server.on("/", handleRoot);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/data", HTTP_GET, handleData);
  server.on("/history", HTTP_GET, handleHistory);
  server.on("/csv", HTTP_GET, handleCSV);
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "Page Not Found");
  });
  
  server.begin();
  Serial.println("Web server started!");
  Serial.println("==========================================");
  
  delay(2000);
  Serial.println("Taking initial sensor readings...");
  int soil = readSensorSmooth(SOIL_PIN);
  int water = readSensorSmooth(WATER_PIN);
  Serial.print("Soil: "); Serial.print(soil);
  Serial.print(" | Water: "); Serial.println(water);
  Serial.println("System ready!");
}

// ===== MAIN LOOP =====
void loop() {
  server.handleClient();
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastSensorCheck >= SENSOR_CHECK_TIME) {
    lastSensorCheck = currentTime;
    
    int soilReading = readSensorSmooth(SOIL_PIN);
    int waterReading = readSensorSmooth(WATER_PIN);
    
    addToHistory(soilReading, waterReading);
    
    if (waterReading >= WATER_GOOD_THRESHOLD) {
      waterLevelGood = true;
    } else if (waterReading < WATER_GOOD_THRESHOLD) {
      waterLevelGood = false;
    }
    
    if (soilReading == SOIL_DRY_THRESHOLD) {
      soilIsDry = true;
    } else {
      soilIsDry = false;
    }
    
    Serial.print("Mode: "); 
    Serial.print(manualMode ? "MANUAL" : (mlAutoMode ? "ML AUTO" : (autoMode ? "AUTO" : "OFF")));
    Serial.print(" | Pump: "); 
    Serial.print(pumpIsRunning ? "ON" : "OFF");
    Serial.print(" | Soil: "); 
    Serial.print(soilReading);
    Serial.print(" ("); 
    Serial.print(soilIsDry ? "DRY" : "WET");
    Serial.print(") | Water: "); 
    Serial.print(waterReading);
    Serial.print(" ("); 
    Serial.print(waterLevelGood ? "GOOD" : "LOW");
    Serial.println(")");
    
    if (mlAutoMode && !pumpIsRunning && currentTime - lastMLCheck >= SENSOR_CHECK_TIME) {
      lastMLCheck = currentTime;
      
      int mlSoilReading = readSensorSmooth(SOIL_PIN);
      int mlWaterReading = readSensorSmooth(WATER_PIN);
      
      String timeStr = String((currentTime / 60000) % 24) + "min";  // Minutes since start (demo time)
      unsigned long lastWatering = (currentTime - lastWaterTime) / 60000;  // Minutes ago
      String lastWateringStr = String(lastWatering) + "_minutes_ago";
      int dryingRate = previousSoil - mlSoilReading;  // Soil change
      String dryingRateStr = (dryingRate > 100) ? "fast" : "slow";
      
      int waterDuration = 0;
      if (mlSoilReading >= 3312) {
        waterDuration = 0;
      } else {
        if (mlWaterReading >= 2001) {
          waterDuration = 0;
        } else {
          waterDuration = 30;
        }
      }
      
      Serial.print("Based on soil=");
      Serial.print(mlSoilReading);
      Serial.print(", water=");
      Serial.print(mlWaterReading);
      Serial.print(", time=");
      Serial.print(timeStr);
      Serial.print(", last_watering=");
      Serial.print(lastWateringStr);
      Serial.print(", drying_rate=");
      Serial.print(dryingRateStr);
      Serial.print(" → Water for ");
      Serial.print(waterDuration);
      Serial.println(" seconds");
      
      if (waterDuration > 0) {
        startPump(waterDuration * 1000);  // Convert to milliseconds
      }
      
      previousSoil = mlSoilReading;
    }
  }
  
  if (manualMode) {
    if (manualPumpState && !pumpIsRunning) {
      startPump(60000);  // 60 seconds max
    } else if (!manualPumpState && pumpIsRunning) {
      stopPump();
    }
  } else if (autoMode) {
    if (pumpIsRunning && !waterLevelGood) {
      stopPump();
      Serial.println("🚨 EMERGENCY STOP - Water level too low!");
    }
    
    if (!pumpIsRunning && waterLevelGood && soilIsDry) {
      startPump(60000);  // 60 seconds max
      Serial.println("🌱 AUTO START - Water good (>=2000) and Soil dry (4095)");
    }
    
    if (pumpIsRunning) {
      bool stopCondition = !(waterLevelGood && soilIsDry);
      bool maxTimeReached = (currentTime - pumpStartTime) >= pumpDuration;
      if (stopCondition || maxTimeReached) {
        stopPump();
        Serial.println("✅ AUTO STOP - Condition not met or max time");
      }
    }
  }
  
  if (mlAutoMode && pumpIsRunning) {
    bool stopCondition = !waterLevelGood;
    bool maxTimeReached = (currentTime - pumpStartTime) >= pumpDuration;
    if (stopCondition || maxTimeReached) {
      stopPump();
      Serial.println("✅ ML AUTO STOP - Condition not met or max time");
    }
  }
  
  int waterReading = readSensorSmooth(WATER_PIN);
  updateLEDs(waterReading);
  updateBuzzer(waterReading);
  
  delay(50);
}
