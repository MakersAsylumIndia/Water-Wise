#include <WiFi.h>

const char* ssid = "Makers Asylum";
const char* password = "Makeithappen";
const int soilPin = 32;
const int relayPin = 18;

WiFiServer server(80);

bool pumpState = false;
bool autoMode = true;
float soilMoisturePercent = 0;
int moistureThreshold = 50; // Now in percentage (0–100)

String webpage = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>Plant Watering Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: 'Segoe UI', sans-serif;
      text-align: center;
      margin: 0;
      padding: 0;
      background: #f4f4f4;
    }

    h1 {
      background-color: #4CAF50;
      color: white;
      margin: 0;
      padding: 20px;
    }

    .container {
      padding: 20px;
    }

    .status {
      font-size: 20px;
      margin-bottom: 20px;
    }

    .button {
      background-color: #4CAF50;
      border: none;
      color: white;
      padding: 15px 25px;
      font-size: 18px;
      margin: 10px;
      border-radius: 10px;
      cursor: pointer;
      box-shadow: 0 5px #999;
    }

    .button:active {
      box-shadow: 0 2px #666;
      transform: translateY(2px);
    }

    label {
      font-size: 18px;
    }

    input[type="range"] {
      width: 60%;
    }

    .moisture-value {
      font-weight: bold;
      color: #2E8B57;
    }
  </style>
</head>
<body>
  <h1>Plant Watering Control</h1>
  <div class="container">
    <p>Soil Moisture: <span id="moisture" class="moisture-value">0</span>%</p>

    <label>
      <input type="checkbox" id="modeSwitch" checked>
      Automatic Mode
    </label>
    <br><br>

    <button id="manualBtn" class="button"> Water Plant Manually</button>

    <p>
      Moisture Threshold: <span id="thresholdValue">50</span>%
    </p>
    <input type="range" id="thresholdSlider" min="0" max="100" value="50">

    <div id="alertBox" style="color:red; font-size:18px; margin-top:15px;"></div>
  </div>

  <script>
    setInterval(function() {
      fetch('/soil').then(response => response.text()).then(data => {
        let moisture = parseInt(data);
        document.getElementById('moisture').textContent = moisture;
        let threshold = parseInt(document.getElementById('thresholdValue').textContent);
        let auto = document.getElementById('modeSwitch').checked;

        if (!auto && moisture < threshold) {
          document.getElementById('alertBox').textContent = " Moisture is low! Water the plant.";
        } else {
          document.getElementById('alertBox').textContent = "";
        }
      });
    }, 2000);

    document.getElementById('modeSwitch').addEventListener('change', function() {
      fetch('/setmode?mode=' + (this.checked ? 'auto' : 'manual'));
    });

    document.getElementById('manualBtn').addEventListener('mousedown', function() {
      fetch('/pump?state=on');
    });

    document.getElementById('manualBtn').addEventListener('mouseup', function() {
      fetch('/pump?state=off');
    });

    document.getElementById('thresholdSlider').addEventListener('input', function() {
      let val = this.value;
      document.getElementById('thresholdValue').textContent = val;
      fetch('/setthreshold?value=' + val);
    });
  </script>
</body>
</html>
)=====";

void setup() {
  Serial.begin(115200);
  pinMode(soilPin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;
  while (!client.available()) delay(1);

  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  if (request.indexOf("GET / ") >= 0) {
    sendResponse(client, webpage);
  } else if (request.indexOf("GET /soil") >= 0) {
    int raw = analogRead(soilPin);
    soilMoisturePercent = 100.0 - ((raw / 4095.0) * 100.0);
    sendResponse(client, String((int)soilMoisturePercent));
  } else if (request.indexOf("GET /setmode?mode=auto") >= 0) {
    autoMode = true;
    sendResponse(client, "Mode set to automatic");
  } else if (request.indexOf("GET /setmode?mode=manual") >= 0) {
    autoMode = false;
    pumpOff();
    sendResponse(client, "Mode set to manual");
  } else if (request.indexOf("GET /pump?state=on") >= 0) {
    if (!autoMode) pumpOn();
    sendResponse(client, "Pump ON");
  } else if (request.indexOf("GET /pump?state=off") >= 0) {
    if (!autoMode) pumpOff();
    sendResponse(client, "Pump OFF");
  } else if (request.indexOf("GET /setthreshold?value=") >= 0) {
    int idx = request.indexOf("value=") + 6;
    moistureThreshold = request.substring(idx).toInt();
    sendResponse(client, "Threshold set to " + String(moistureThreshold));
  } else {
    sendResponse(client, "Unknown request");
  }

  if (autoMode) {
    int raw = analogRead(soilPin);
    soilMoisturePercent = 100.0 - ((raw / 4095.0) * 100.0);
    if (soilMoisturePercent < moistureThreshold) {
      pumpOn();
      Serial.println("WATERED_ALERT");
    } else {
      pumpOff();
    }
  }
}

void sendResponse(WiFiClient& client, String content) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println(content);
}

void pumpOn() {
  digitalWrite(relayPin, LOW);
  pumpState = true;
  Serial.println("Pump ON");
}

void pumpOff() {
  digitalWrite(relayPin, HIGH);
  pumpState = false;
  Serial.println("Pump OFF");
}