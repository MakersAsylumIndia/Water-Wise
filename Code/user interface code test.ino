#include <WiFi.h>
#include <ArduinoJson.h> // For creating JSON responses. Install via Library Manager.

// --- WiFi Credentials ---
const char* ssid = "YOUR_WIFI_SSID";       // <<<<<<<<<<<< CHANGE THIS
const char* password = "YOUR_WIFI_PASSWORD"; // <<<<<<<<<<<< CHANGE THIS

// --- Hardware Pins ---
const int soilPin = 32;     // Analog input pin for soil moisture sensor (ESP32 ADC1 can use GPIOs 32-39)
const int relayPin = 18;    // Digital output pin for the relay

// --- Plant Identification (for the dashboard) ---
const int PLANT_ID = 1; // Unique ID for the plant this ESP32 manages
const char* PLANT_NAME = "My ESP32 Plant"; // Name for this plant

WiFiServer server(80);

// --- State Variables ---
bool pumpState = false;     // Current state of the pump (true = ON, false = OFF)
bool autoMode = true;       // true for automatic watering, false for manual
float soilMoisturePercent = 0; // Stores the latest calculated moisture percentage
int moistureThreshold = 50; // Default threshold percentage (0-100) for automatic watering

// --- WaterWise Dashboard HTML ---
// This is the full HTML, CSS, and JavaScript for the user interface.
// It's designed to fetch data from and send commands to this ESP32.
String waterWiseDashboardHTML = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>WaterWise ESP Control</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen,
        Ubuntu, Cantarell, "Open Sans", "Helvetica Neue", sans-serif;
      margin: 0;
      padding: 20px;
      background: #f4f8fb;
      color: #555;
    }
    h1 {
      text-align: center;
      font-weight: 600;
      color: #444;
      margin-bottom: 1rem;
    }
    .container {
      display: flex;
      justify-content: center; /* Center items for single-plant view */
      gap: 40px;
      flex-wrap: wrap;
    }
    .sidebar {
      width: 220px;
      min-width: 220px;
    }
    .plant-list {
      list-style: none;
      padding: 0;
      border: 1px solid #ccc;
      border-radius: 5px;
      background: white;
      max-height: 400px;
      overflow-y: auto;
      color: #444;
    }
    .plant-list li {
      padding: 10px 15px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-bottom: 1px solid #eee;
      cursor: pointer;
      user-select: none;
      font-weight: 500;
      transition: background-color 0.25s ease;
    }
    .plant-list li:last-child {
      border-bottom: none;
    }
    .plant-list li.active {
      background-color: #d0f0d0;
      font-weight: 600;
    }
    .plant-list li.empty-message {
        justify-content: center;
        cursor: default;
        color: #888;
    }
    .warning {
      color: #d9534f;
      font-weight: 600;
      margin-left: 8px;
      user-select: none;
    }
    .progress, .threshold-bar {
      height: 200px;
      width: 80px;
      border: 1px solid #ccc;
      position: relative;
      margin: 20px auto;
      border-radius: 5px;
      background-color: white;
      user-select: none;
      box-shadow: none;
    }
    .progress-fill, .threshold-fill {
      position: absolute;
      bottom: 0;
      width: 100%;
      border-radius: 0 0 5px 5px;
      font-weight: 600;
      line-height: 200px;
      transition: height 0.5s ease;
      display: flex;
      justify-content: center;
      align-items: center;
      white-space: nowrap;
      user-select: none;
      font-feature-settings: "tnum";
      font-variant-numeric: tabular-nums;
    }
    .progress-fill {
      background-color: steelblue;
      color: white;
    }
    .threshold-fill {
      background-color: #28a745;
      color: white;
    }
    .label-above {
      position: absolute;
      bottom: 100%;
      width: 100%;
      text-align: center;
      font-weight: 600;
      color: #777;
      user-select: none;
      margin-bottom: 4px;
      font-feature-settings: "tnum";
      font-variant-numeric: tabular-nums;
      font-size: 0.9rem;
    }
    .controls {
      width: 220px;
      min-width: 220px;
      text-align: center;
      padding: 20px 0 0 0;
      background: transparent;
      box-shadow: none;
      color: #555;
      user-select: none;
    }
    .switch {
      position: relative;
      display: inline-block;
      width: 50px;
      height: 28px;
      margin-bottom: 10px;
      vertical-align: middle;
    }
    .switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0; left: 0; right: 0; bottom: 0;
      background-color: #ccc;
      border-radius: 34px;
      transition: 0.4s;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 22px;
      width: 22px;
      left: 3px;
      bottom: 3px;
      background-color: white;
      border-radius: 50%;
      transition: 0.4s;
      box-shadow: 0 1px 3px rgba(0,0,0,0.12);
    }
    input:checked + .slider {
      background-color: #4cd964;
    }
    input:focus + .slider {
      box-shadow: 0 0 1px #4cd964;
    }
    input:checked + .slider:before {
      transform: translateX(22px);
    }
    .mode-label {
      display: inline-block;
      margin-left: 12px;
      font-weight: 500;
      font-size: 1rem;
      vertical-align: middle;
      user-select: none;
      color: #555;
      letter-spacing: 0.03em;
    }
    button {
      background-color: #1e88e5;
      color: white;
      padding: 10px 15px;
      border: none;
      cursor: pointer;
      border-radius: 5px;
      font-weight: 600;
      margin-top: 10px;
      width: 100%;
      user-select: none;
      transition: background-color 0.3s ease, display 0s ease 0.1s; /* Added display transition */
      font-feature-settings: "tnum";
      font-variant-numeric: tabular-nums;
      font-size: 1rem;
      box-shadow: none;
    }
    button:hover {
      background-color: #1565c0;
    }
    button:disabled {
      background-color: #ccc;
      cursor: not-allowed;
    }
    input[type="range"] {
      width: 100%;
      margin-top: 5px;
      cursor: pointer;
    }
    input[type="range"]:disabled {
        cursor: not-allowed;
        opacity: 0.6;
    }
    h3 {
      color: #555;
      font-weight: 600;
      margin-bottom: 0.5rem;
      user-select: none;
      letter-spacing: 0.02em;
    }
    h4 {
      color: #666;
      font-weight: 500;
      margin-top: 20px;
      margin-bottom: 6px;
      user-select: none;
      letter-spacing: 0.02em;
    }
  </style>
</head>
<body>
  <h1>WaterWise ESP Control</h1>
  <div class="container">
    <div class="sidebar">
      <h3>Plant List</h3>
      <ul class="plant-list" id="plantList">
        {/* Plants will be dynamically inserted here by JS fetching /data */}
      </ul>
    </div>

    <div>
      <h3 style="text-align:center">Current Moisture</h3>
      <div class="progress" aria-label="Current moisture level">
        <div id="moistureFill" class="progress-fill" style="height: 0%">0%</div>
        <div id="moistureLabelAbove" class="label-above" style="display:none">0%</div>
      </div>
    </div>

    <div>
      <h3 style="text-align:center">Threshold Moisture</h3>
      <div class="threshold-bar" aria-label="Moisture threshold">
        <div id="thresholdFill" class="threshold-fill" style="height: 50%">50%</div>
        <div id="thresholdLabelAbove" class="label-above" style="display:none">50%</div>
      </div>
    </div>

    <div class="controls">
      <h3>Watering Mode:</h3>
      <label class="switch" aria-label="Toggle watering mode">
        <input type="checkbox" id="modeToggle" />
        <span class="slider"></span>
      </label>
      <span id="modeLabel" class="mode-label">Manual</span>

      <button id="waterNowBtn" aria-label="Water plant now">WATER NOW</button>

      <h4>Set Threshold:</h4>
      <input type="range" min="0" max="100" value="50" id="thresholdSlider" aria-valuemin="0" aria-valuemax="100" aria-valuenow="50" />
    </div>
  </div>

  <script>
    const plantListElem = document.getElementById("plantList");
    const moistureFill = document.getElementById("moistureFill");
    const moistureLabelAbove = document.getElementById("moistureLabelAbove");
    const thresholdFill = document.getElementById("thresholdFill");
    const thresholdLabelAbove = document.getElementById("thresholdLabelAbove");
    const modeToggle = document.getElementById("modeToggle");
    const modeLabel = document.getElementById("modeLabel");
    const waterNowBtn = document.getElementById("waterNowBtn");
    const thresholdSlider = document.getElementById("thresholdSlider");

    const plants = []; 
    let selectedPlantIndex = -1; 

    function renderPlantList() {
      plantListElem.innerHTML = "";
      if (plants.length === 0) {
        const li = document.createElement("li");
        li.textContent = "Waiting for plant data...";
        li.classList.add("empty-message");
        plantListElem.appendChild(li);
        return;
      }
      plants.forEach((plant, idx) => {
        const li = document.createElement("li");
        li.textContent = plant.name;
        if (plant.moisture < plant.threshold) {
          const warningSpan = document.createElement("span");
          warningSpan.className = "warning";
          warningSpan.textContent = "⚠";
          li.appendChild(warningSpan);
        }
        if (idx === selectedPlantIndex) {
          li.classList.add("active");
        }
        li.onclick = () => {
          selectPlant(idx);
        };
        plantListElem.appendChild(li);
      });
    }

    function updateBars() {
      const plant = selectedPlantIndex >=0 ? plants[selectedPlantIndex] : null;

      if (plant) {
        updateMoistureBar(plant.moisture);
        updateThresholdBar(plant.threshold);
        updateModeUI(plant.mode); 
        thresholdSlider.disabled = false;
        modeToggle.disabled = false;
      } else { 
        updateMoistureBar(0);
        updateThresholdBar(50); 
        updateModeUI("manual"); 
        modeToggle.checked = false;
        waterNowBtn.disabled = true; // Explicitly disable if no plant, even if visible
        thresholdSlider.disabled = true;
        thresholdSlider.value = 50;
        thresholdSlider.setAttribute("aria-valuenow", 50);
        modeToggle.disabled = true;
      }
    }

    function updateMoistureBar(value) {
      moistureFill.style.height = value + "%";
      moistureFill.textContent = value + "%";
      moistureLabelAbove.style.display = (value < 7) ? "block" : "none";
      if (value < 7) moistureLabelAbove.textContent = value + "%";
      if (value < 7) moistureFill.textContent = ""; 
    }

    function updateThresholdBar(value) {
      thresholdFill.style.height = value + "%";
      thresholdFill.textContent = value + "%";
      thresholdSlider.value = value; 
      thresholdSlider.setAttribute("aria-valuenow", value);
      thresholdLabelAbove.style.display = (value < 7) ? "block" : "none";
      if (value < 7) thresholdLabelAbove.textContent = value + "%";
      if (value < 7) thresholdFill.textContent = ""; 
    }

    function updateModeUI(mode) {
      modeLabel.textContent = mode === "auto" ? "Auto" : "Manual";
      modeToggle.checked = mode === "auto";
      
      if (mode === "auto") {
        waterNowBtn.style.display = "none"; 
        waterNowBtn.disabled = true; 
      } else { // Manual mode
        waterNowBtn.style.display = "";     
        // Enable/disable based on whether a plant is selected
        waterNowBtn.disabled = (selectedPlantIndex < 0 || plants.length === 0); 
      }
    }

    function selectPlant(idx) {
      if (idx < 0 || idx >= plants.length) {
        selectedPlantIndex = -1; 
      } else {
        selectedPlantIndex = idx;
      }
      renderPlantList();
      updateBars(); 
    }

    modeToggle.addEventListener("change", () => {
      let currentPlant = selectedPlantIndex >= 0 ? plants[selectedPlantIndex] : null;
      
      if (!currentPlant && plants.length > 0 && selectedPlantIndex === -1) {
          selectPlant(0); // Attempt to select the first plant if none is selected
          currentPlant = selectedPlantIndex >= 0 ? plants[selectedPlantIndex] : null; // Re-evaluate
      }
      
      if (!currentPlant) {
          console.warn("Mode toggle changed but no plant is selected or available.");
          // Optionally revert toggle state if this happens unexpectedly
          // modeToggle.checked = !modeToggle.checked; 
          return; 
      }

      const newMode = modeToggle.checked ? "auto" : "manual";
      fetch(\/setmode?mode=\${newMode}&plantId=\${currentPlant.id}\)
        .then(response => response.text())
        .then(text => {
            console.log("Set mode response:", text);
            currentPlant.mode = newMode; 
            updateModeUI(currentPlant.mode); 
        })
        .catch(err => console.error("Error setting mode:", err));
    });

    waterNowBtn.addEventListener("click", () => {
      const plant = selectedPlantIndex >=0 ? plants[selectedPlantIndex] : null;
      if (!plant) return; 
      if (plant.mode === "auto") return; // Button should be hidden, but good check

      fetch(\/pump?state=on&plantId=\${plant.id}\)
        .then(response => response.text())
        .then(text => {
            console.log("Water now response:", text);
        })
        .catch(err => console.error("Error watering now:", err));
    });

    thresholdSlider.addEventListener("input", () => {
      const plant = selectedPlantIndex >=0 ? plants[selectedPlantIndex] : null;
      if (!plant) return;

      const newThreshold = parseInt(thresholdSlider.value, 10);
      fetch(\/setthreshold?value=\${newThreshold}&plantId=\${plant.id}\)
        .then(response => response.text())
        .then(text => {
            console.log("Set threshold response:", text);
            plant.threshold = newThreshold; 
            updateThresholdBar(plant.threshold);
            renderPlantList(); 
        })
        .catch(err => console.error("Error setting threshold:", err));
    });

    window.setMoistureValueForPlant = function(plantId, moistureValue, plantName, currentThreshold, currentMode) {
      let plant = plants.find(p => p.id === plantId);
      const isNewPlant = !plant;

      if (isNewPlant) {
        plant = {
          id: plantId,
          name: plantName || \Plant \${plantId}\, 
          moisture: moistureValue,
          threshold: currentThreshold,
          mode: currentMode
        };
        plants.push(plant);
        if (plants.length === 1 && selectedPlantIndex === -1) { 
            selectPlant(0); 
        }
      } else { 
        plant.moisture = moistureValue;
        plant.threshold = currentThreshold; 
        plant.mode = currentMode;        
        if (plantName) plant.name = plantName; 
      }
      
      renderPlantList(); 

      if ((selectedPlantIndex >=0 && plants[selectedPlantIndex] && plants[selectedPlantIndex].id === plantId)) {
         updateBars(); 
      } else if (isNewPlant && plants.length === 1 && selectedPlantIndex !== 0) {
         if(plants[0].id === plantId) selectPlant(0);
      } else if (plants.length > 0 && selectedPlantIndex === -1) { // If no plant was selected yet but now we have data
         selectPlant(0);
      }
    }
    
    function fetchDataFromESP() {
        fetch('/data') 
            .then(response => {
                if (!response.ok) {
                    throw new Error('Network response was not ok: ' + response.statusText);
                }
                return response.json(); 
            })
            .then(data => {
                if (data && typeof data.plantId !== 'undefined') {
                    window.setMoistureValueForPlant(data.plantId, data.moisture, data.name, data.threshold, data.mode);
                } else {
                    console.warn("Received malformed or incomplete data from ESP:", data);
                }
            })
            .catch(err => {
                console.error("Error fetching data from ESP:", err);
                if (plants.length === 0) { 
                    plantListElem.innerHTML = '<li class="empty-message">Error connecting to ESP. Check console.</li>';
                }
            });
    }

    fetchDataFromESP(); 
    setInterval(fetchDataFromESP, 5000); 

    renderPlantList();
    updateBars(); 
  </script></body></html>
)=====";


// --- Function Declarations (Prototypes) ---
void sendHtmlResponse(WiFiClient& client, const String& content);
void sendTextResponse(WiFiClient& client, const String& content, const String& contentType = "text/plain", int statusCode = 200);
void sendDataResponse(WiFiClient& client);
void handleSetMode(WiFiClient& client, const String& request);
void handlePumpControl(WiFiClient& client, const String& request);
void handleSetThreshold(WiFiClient& client, const String& request);
void pumpOn();
void pumpOff();
void checkAndWaterAutomatically();


void setup() {
  Serial.begin(115200);
  pinMode(soilPin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); // Pump OFF initially (HIGH for many common relay modules)

  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("ESP32 IP Address: http://");
    Serial.println(WiFi.localIP());
    server.begin();
    Serial.println("HTTP server started. Open browser to the IP address above.");
  } else {
    Serial.println("\nFailed to connect to WiFi. Please check credentials or network.");
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected. Attempting to reconnect...");
    WiFi.begin(ssid, password);
    delay(1000);
    return;
  }

  WiFiClient client = server.available();
  if (!client) {
    if (autoMode) {
      checkAndWaterAutomatically();
    }
    delay(100);
    return;
  }

  Serial.println("\nNew client connected.");
  unsigned long currentTime = millis();
  unsigned long previousTime = currentTime;
  String currentLine = "";
  String firstRequestLine = "";
  bool firstLineRead = false;

  while (client.connected() && (millis() - previousTime <= 3000)) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') {
        if (!firstLineRead && currentLine.length() > 0) {
            firstRequestLine = currentLine;
            firstRequestLine.trim();
            Serial.print("Request Line: "); Serial.println(firstRequestLine);
            firstLineRead = true;
        }
        if (currentLine.length() == 0) { // End of HTTP Headers
          if (firstRequestLine.startsWith("GET / ")) {
            sendHtmlResponse(client, waterWiseDashboardHTML);
          } else if (firstRequestLine.startsWith("GET /data")) {
            sendDataResponse(client);
          } else if (firstRequestLine.startsWith("GET /setmode")) {
            handleSetMode(client, firstRequestLine);
          } else if (firstRequestLine.startsWith("GET /pump")) {
            handlePumpControl(client, firstRequestLine);
          } else if (firstRequestLine.startsWith("GET /setthreshold")) {
            handleSetThreshold(client, firstRequestLine);
          } else {
            if (firstRequestLine.startsWith("GET /favicon.ico")) {
                sendTextResponse(client, "Favicon not found", "text/plain", 404);
            } else {
                Serial.print("Unknown request: "); Serial.println(firstRequestLine);
                sendTextResponse(client, "404 Not Found - Unknown Endpoint", "text/plain", 404);
            }
          }
          break; // Request processed
        }
        currentLine = ""; // Reset for next header line or empty line
      } else if (c != '\r') { // Ignore CR, accumulate other chars
        currentLine += c;
      }
      previousTime = millis(); // Reset timeout while receiving data
    }
  }
  
  if (!firstLineRead && currentLine.length() > 0) { // Client disconnected before sending full headers
    firstRequestLine = currentLine;
    firstRequestLine.trim();
    Serial.print("Partial Request Line (client disconnected early?): "); Serial.println(firstRequestLine);
  }

  client.stop();
  Serial.println("Client disconnected.");
  delay(10);
}

void sendHtmlResponse(WiFiClient& client, const String& content) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println("Content-Length: " + String(content.length()));
  client.println();
  client.print(content);
  client.flush();
}

void sendTextResponse(WiFiClient& client, const String& content, const String& contentType, int statusCode) {
  client.print("HTTP/1.1 "); client.print(statusCode);
  client.println(statusCode == 200 ? " OK" : (statusCode == 404 ? " Not Found" : " Bad Request"));
  client.print("Content-Type: "); client.println(contentType);
  client.println("Connection: close");
  client.println("Content-Length: " + String(content.length()));
  client.println();
  client.println(content);
  client.flush();
}

void sendDataResponse(WiFiClient& client) {
  int raw = analogRead(soilPin);

  // --- !!! CRITICAL CALIBRATION STEP !!! ---
  int dryValue = 3300; // EXAMPLE: ADC reading when sensor is fairly dry <<<<<<< CALIBRATE
  int wetValue = 1300; // EXAMPLE: ADC reading when sensor is in water   <<<<<<< CALIBRATE
  // If dryValue > wetValue (typical for capacitive sensors):
  soilMoisturePercent = map(raw, dryValue, wetValue, 0, 100);
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);

  StaticJsonDocument<256> doc;
  doc["plantId"] = PLANT_ID;
  doc["name"] = PLANT_NAME;
  doc["moisture"] = (int)soilMoisturePercent;
  doc["threshold"] = moistureThreshold;
  doc["mode"] = autoMode ? "auto" : "manual";

  String jsonOutput;
  serializeJson(doc, jsonOutput);

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println("Content-Length: " + String(jsonOutput.length()));
  client.println("Cache-Control: no-cache");
  client.println();
  client.println(jsonOutput);
  client.flush();
}

void handleSetMode(WiFiClient& client, const String& request) {
  if (request.indexOf("mode=auto") > 0) {
    autoMode = true;
    Serial.println("Mode changed to: Auto");
    sendTextResponse(client, "Mode set to Auto");
  } else if (request.indexOf("mode=manual") > 0) {
    autoMode = false;
    pumpOff();
    Serial.println("Mode changed to: Manual");
    sendTextResponse(client, "Mode set to Manual");
  } else {
    sendTextResponse(client, "Invalid mode parameter", "text/plain", 400);
  }
}

void handlePumpControl(WiFiClient& client, const String& request) {
  if (request.indexOf("state=on") > 0) {
    if (!autoMode) {
        pumpOn();
        sendTextResponse(client, "Pump ON (Manual)");
    } else {
        Serial.println("Pump ON command received in Auto mode.");
        // Optional: could implement a short timed burst here even in auto
        // pumpOn(); delay(2000); pumpOff(); 
        sendTextResponse(client, "Pump command noted (Auto Mode)");
    }
  } else if (request.indexOf("state=off") > 0) { // Not typically called by current UI for WATER NOW
    if (!autoMode) {
        pumpOff();
        sendTextResponse(client, "Pump OFF (Manual)");
    } else {
        sendTextResponse(client, "Pump OFF ignored (Auto Mode)");
    }
  } else {
    sendTextResponse(client, "Invalid pump state parameter", "text/plain", 400);
  }
}

void handleSetThreshold(WiFiClient& client, const String& request) {
  int valIndex = request.indexOf("value=");
  if (valIndex > 0) {
    int endIndex = request.indexOf('&', valIndex);
    if (endIndex == -1) {
        endIndex = request.length();
    }
    String valStr = request.substring(valIndex + 6, endIndex);
    
    moistureThreshold = valStr.toInt();
    moistureThreshold = constrain(moistureThreshold, 0, 100);
    Serial.print("Moisture threshold set to: "); Serial.println(moistureThreshold);
    sendTextResponse(client, "Threshold set to " + String(moistureThreshold));
  } else {
    sendTextResponse(client, "Invalid threshold value parameter", "text/plain", 400);
  }
}

void pumpOn() {
  if (!pumpState) {
    digitalWrite(relayPin, LOW); 
    pumpState = true;
    Serial.println("Pump turned ON");
  } else {
    Serial.println("Pump already ON");
  }
}

void pumpOff() {
  if (pumpState) {
    digitalWrite(relayPin, HIGH); 
    pumpState = false;
    Serial.println("Pump turned OFF");
  } else {
    Serial.println("Pump already OFF");
  }
}

void checkAndWaterAutomatically() {
  static unsigned long lastAutoWaterCheckTime = 0;
  const unsigned long autoWaterCheckInterval = 10000; // Check every 10 seconds 

  if (millis() - lastAutoWaterCheckTime >= autoWaterCheckInterval) {
    lastAutoWaterCheckTime = millis();

    int raw = analogRead(soilPin);
    int dryValue = 3300; // <<<<< CALIBRATE THIS
    int wetValue = 1300; // <<<<< CALIBRATE THIS
    
    float currentMoisture = map(raw, dryValue, wetValue, 0, 100);
    currentMoisture = constrain(currentMoisture, 0, 100);

    Serial.print("Auto mode check: Moisture="); Serial.print((int)currentMoisture);
    Serial.print("%, Threshold="); Serial.println(moistureThreshold);

    if (currentMoisture < moistureThreshold) {
      Serial.println("Auto: Moisture below threshold. Turning pump ON.");
      pumpOn();
    } else {
      // Serial.println("Auto: Moisture at or above threshold. Ensuring pump is OFF.");
      pumpOff();
    }
  }
}