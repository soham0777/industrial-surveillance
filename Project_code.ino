// ============================================================
// Farm Security System – XIAO ESP32-S3 Cam
// Fixed servo movement + responsive controls
// ============================================================

#include "esp_camera.h"
#include <esp_http_server.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ==================== Wi-Fi Credentials ====================
const char* ssid = "sk";
const char* password = "12345678";

// ==================== Camera Pins (XIAO ESP32-S3 Sense) ====================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// ==================== Corrected Peripheral Pins ====================
#define DHTPIN            1   // D0 (GPIO1)
#define DHTTYPE           DHT11
#define LDRPIN            2   // D1 (GPIO2)
#define RAINPIN           3   // D2 (GPIO3) – LOW = rain
#define PIRPIN            4   // D3 (GPIO4)
#define SERVOPIN          5   // D4 (GPIO5)
#define BUZZERPIN         6   // D5 (GPIO6)
#define LED_BLUE          7   // D8 (GPIO7)
#define LED_RED           8   // D9 (GPIO8)

// ==================== Global Objects ====================
DHT dht(DHTPIN, DHTTYPE);
Servo servo;
WebServer server(80);

float temperature = 0, humidity = 0;
int lightValue = 0;
bool rainDetected = false;
bool motionDetected = false;
bool autoPanEnabled = true;
int servoPanSpeed = 95;   // 90=stop, >90 right, <90 left; start with 95 (slow right)

unsigned long lastMotionTime = 0;
unsigned long lastSensorRead = 0;

// ==================== Camera Initialization (Higher FPS) ====================
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_HVGA;   // 480x320
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("Camera OK (HVGA, quality=10)");
}

// ==================== Camera Stream Handler ====================
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          Serial.println("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, "Content-Length: %u\r\n\r\n", _jpg_buf_len);
      if (httpd_resp_send_chunk(req, part_buf, hlen) == ESP_OK) {
        if (httpd_resp_send_chunk(req, (const char*)_jpg_buf, _jpg_buf_len) == ESP_OK) {
          if (httpd_resp_send_chunk(req, "--frame\r\n", 9) == ESP_OK) {
            res = ESP_OK;
          }
        }
      }
    }

    if (fb) esp_camera_fb_return(fb);
    if (_jpg_buf && fb == NULL) free(_jpg_buf);
    if (res != ESP_OK) break;
    delay(1);
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  httpd_handle_t stream_httpd = NULL;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_uri_t stream_uri = {
      .uri       = "/stream",
      .method    = HTTP_GET,
      .handler   = stream_handler,
      .user_ctx  = NULL
    };
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("Camera server started on port 81");
  } else {
    Serial.println("Failed to start camera server");
  }
}

// ==================== Web Page (Modern UI with servo controls) ====================
String getHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=yes">
    <title>FarmGuard Pro</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            background: linear-gradient(145deg, #0a0f0e 0%, #1a2a22 100%);
            font-family: 'Inter', -apple-system, 'Segoe UI', Roboto, sans-serif;
            padding: 20px;
            min-height: 100vh;
            color: #eef4ff;
        }
        .container { max-width: 1400px; margin: 0 auto; }
        h1 {
            text-align: center;
            font-size: 2.2rem;
            background: linear-gradient(135deg, #d4f1f9, #a0e7b0);
            -webkit-background-clip: text;
            background-clip: text;
            color: transparent;
            margin-bottom: 20px;
        }
        .video-container {
            background: #000;
            border-radius: 28px;
            overflow: hidden;
            margin-bottom: 24px;
            box-shadow: 0 20px 35px -10px rgba(0,0,0,0.5);
            border: 1px solid rgba(80,160,100,0.3);
        }
        .video-container img { width: 100%; display: block; }
        .dashboard {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
            gap: 18px;
            margin-bottom: 28px;
        }
        .card {
            background: rgba(25, 35, 30, 0.75);
            backdrop-filter: blur(12px);
            border-radius: 28px;
            padding: 20px 12px;
            text-align: center;
            border: 1px solid rgba(80,160,100,0.3);
            transition: 0.2s;
        }
        .card:hover { transform: translateY(-4px); background: rgba(30,45,38,0.85); }
        .card h3 { font-size: 1rem; text-transform: uppercase; color: #b8f2c2; margin-bottom: 12px; }
        .card p { font-size: 2rem; font-weight: 700; color: white; }
        .motion-alert p { color: #ff8a7a; animation: pulse 0.8s infinite; }
        @keyframes pulse {
            0% { opacity: 1; text-shadow: 0 0 0 #ff4a2a; }
            50% { opacity: 0.85; text-shadow: 0 0 12px #ff4a2a; }
            100% { opacity: 1; text-shadow: 0 0 0 #ff4a2a; }
        }
        .controls {
            display: flex;
            flex-wrap: wrap;
            gap: 12px;
            justify-content: center;
            margin-bottom: 28px;
        }
        button {
            background: rgba(20,28,24,0.9);
            border: 1px solid rgba(100,200,120,0.5);
            padding: 12px 24px;
            border-radius: 60px;
            font-size: 0.95rem;
            font-weight: 600;
            color: #e0f0e4;
            cursor: pointer;
            transition: 0.2s;
        }
        button:hover { background: #2a5a3a; border-color: #8affaa; transform: scale(1.02); }
        button.primary { background: #2c6e3c; border-color: #a0ffb0; color: white; }
        button.warning { background: #9e3c2a; border-color: #ff9a7a; }
        .speed-slider {
            display: flex;
            align-items: center;
            gap: 15px;
            background: rgba(0,0,0,0.5);
            padding: 8px 20px;
            border-radius: 40px;
        }
        .speed-slider label { font-weight: bold; }
        input { width: 200px; cursor: pointer; }
        .log {
            background: rgba(0,0,0,0.6);
            backdrop-filter: blur(8px);
            border-radius: 24px;
            padding: 16px 20px;
            font-family: monospace;
            font-size: 0.85rem;
            max-height: 200px;
            overflow-y: auto;
            border: 1px solid rgba(100,200,120,0.3);
            color: #bbffcc;
        }
        .log p { margin: 6px 0; border-left: 2px solid #4caf50; padding-left: 12px; }
        @media (max-width: 680px) {
            .card p { font-size: 1.4rem; }
            button { padding: 8px 16px; font-size: 0.85rem; }
            h1 { font-size: 1.6rem; }
        }
        footer { text-align: center; margin-top: 30px; font-size: 0.75rem; opacity: 0.6; }
    </style>
</head>
<body>
<div class="container">
    <h1>🌾 FarmGuard | 360° Security</h1>
    <div class="video-container">
        <img id="stream" src="http://)rawliteral";
  html += WiFi.localIP().toString();
  html += R"rawliteral(:81/stream" alt="Live 360° View">
    </div>
    <div class="dashboard" id="sensors">
        <div class="card"><h3>🌡️ TEMP</h3><p id="temp">--</p></div>
        <div class="card"><h3>💧 HUMIDITY</h3><p id="hum">--</p></div>
        <div class="card"><h3>☀️ LIGHT</h3><p id="light">--</p></div>
        <div class="card"><h3>🌧️ RAIN</h3><p id="rain">--</p></div>
        <div class="card" id="motionCard"><h3>🚨 MOTION</h3><p id="motion">--</p></div>
    </div>
    <div class="controls">
        <button class="primary" onclick="manualControl('left')">◀ LEFT</button>
        <button class="primary" onclick="manualControl('right')">RIGHT ▶</button>
        <button class="primary" onclick="manualControl('stop')">⏹️ STOP</button>
        <button class="primary" id="autoPanBtn" onclick="toggleAutoPan()">🔄 AUTO PAN (ON)</button>
    </div>
    <div class="controls">
        <div class="speed-slider">
            <label>🐌 Pan Speed</label>
            <input type="range" id="speedSlider" min="0" max="180" value="95" step="1" oninput="updateSpeed(this.value)">
            <span id="speedValue">95</span>
        </div>
    </div>
    <div class="controls">
        <button onclick="setLED('blue',1)">🔵 BLUE ON</button>
        <button onclick="setLED('blue',0)">🔵 BLUE OFF</button>
        <button onclick="setLED('red',1)">🔴 RED ON</button>
        <button onclick="setLED('red',0)">🔴 RED OFF</button>
        <button class="warning" onclick="setBuzzer(1)">🔊 BUZZER ON</button>
        <button class="warning" onclick="setBuzzer(0)">🔇 BUZZER OFF</button>
    </div>
    <div class="log" id="log">
        <p>🟢 System online • Servo test completed</p>
    </div>
    <footer>FarmGuard Pro | Real‑time sensors | Responsive servo</footer>
</div>
<script>
    let lastMotion = false;
    let autoPanActive = true;
    let currentSpeed = 95;
    const autoPanBtn = document.getElementById('autoPanBtn');
    const speedSlider = document.getElementById('speedSlider');
    const speedValue = document.getElementById('speedValue');

    function fetchSensors() {
        fetch('/api/sensors')
            .then(r => r.json())
            .then(data => {
                document.getElementById('temp').innerHTML = data.temp + "°C";
                document.getElementById('hum').innerHTML = data.hum + "%";
                document.getElementById('light').innerHTML = data.light;
                document.getElementById('rain').innerHTML = data.rain ? "🌧 WET" : "☀️ DRY";
                const motionElem = document.getElementById('motion');
                const motionCard = document.getElementById('motionCard');
                if (data.motion) {
                    motionElem.innerHTML = "⚠️ DETECTED";
                    motionCard.classList.add('motion-alert');
                    if (!lastMotion) addLog("🚨 MOTION ALERT!");
                } else {
                    motionElem.innerHTML = "✅ CLEAR";
                    motionCard.classList.remove('motion-alert');
                }
                lastMotion = data.motion;
            })
            .catch(e => console.log(e));
    }

    function addLog(msg) {
        const logDiv = document.getElementById('log');
        const p = document.createElement('p');
        p.innerHTML = `⏱️ ${new Date().toLocaleTimeString()} &nbsp; ${msg}`;
        logDiv.appendChild(p);
        logDiv.scrollTop = logDiv.scrollHeight;
        while (logDiv.children.length > 35) logDiv.removeChild(logDiv.firstChild);
    }

    function manualControl(dir) {
        fetch('/api/manual?dir='+dir)
            .then(() => {
                if (dir !== 'stop') autoPanActive = false;
                autoPanBtn.innerHTML = autoPanActive ? "🔄 AUTO PAN (ON)" : "⏸️ AUTO PAN (OFF)";
                addLog(`Manual: ${dir}`);
            })
            .catch(e => console.log(e));
    }

    function toggleAutoPan() {
        fetch('/api/auto_pan?state=' + (autoPanActive ? 0 : 1))
            .then(() => {
                autoPanActive = !autoPanActive;
                autoPanBtn.innerHTML = autoPanActive ? "🔄 AUTO PAN (ON)" : "⏸️ AUTO PAN (OFF)";
                addLog(autoPanActive ? "Auto‑pan resumed" : "Auto‑pan stopped");
                if (autoPanActive) {
                    // Re-apply current speed
                    fetch('/api/speed?value=' + currentSpeed);
                }
            })
            .catch(e => console.log(e));
    }

    function updateSpeed(val) {
        currentSpeed = parseInt(val);
        speedValue.innerText = currentSpeed;
        fetch('/api/speed?value=' + currentSpeed)
            .then(() => addLog(`Pan speed set to ${currentSpeed}`))
            .catch(e => console.log(e));
    }

    function setLED(color, state) {
        fetch('/api/led?'+color+'='+state)
            .then(() => addLog(`LED ${color.toUpperCase()} ${state ? "ON" : "OFF"}`))
            .catch(e => console.log(e));
    }

    function setBuzzer(state) {
        fetch('/api/buzzer?state='+state)
            .then(() => addLog(`Buzzer ${state ? "activated" : "silenced"}`))
            .catch(e => console.log(e));
    }

    setInterval(fetchSensors, 2000);
    fetchSensors();
    const streamImg = document.getElementById('stream');
    streamImg.onerror = () => addLog("⚠️ Camera stream lost");
    streamImg.onload = () => addLog("📷 Camera stream stable");
</script>
</body>
</html>
)rawliteral";
  return html;
}

// ==================== API Endpoints ====================
void setupAPI() {
  server.on("/api/sensors", HTTP_GET, []() {
    String json = "{\"temp\":" + String(temperature) + ",\"hum\":" + String(humidity) +
                  ",\"light\":" + String(lightValue) + ",\"rain\":" + String(rainDetected ? 1 : 0) +
                  ",\"motion\":" + String(motionDetected ? 1 : 0) + "}";
    server.send(200, "application/json", json);
  });

  // Manual control
  server.on("/api/manual", HTTP_GET, []() {
    if (server.hasArg("dir")) {
      String dir = server.arg("dir");
      if (dir == "left") {
        servo.write(70);    // faster left
        autoPanEnabled = false;
      } else if (dir == "right") {
        servo.write(110);   // faster right
        autoPanEnabled = false;
      } else if (dir == "stop") {
        servo.write(90);
        autoPanEnabled = false;
      }
    }
    server.send(200, "text/plain", "OK");
  });

  // Auto-pan toggle
  server.on("/api/auto_pan", HTTP_GET, []() {
    if (server.hasArg("state")) {
      int state = server.arg("state").toInt();
      if (state == 1) {
        servo.write(servoPanSpeed);
        autoPanEnabled = true;
      } else {
        servo.write(90);
        autoPanEnabled = false;
      }
    }
    server.send(200, "text/plain", "OK");
  });

  // Set pan speed (for auto-pan)
  server.on("/api/speed", HTTP_GET, []() {
    if (server.hasArg("value")) {
      servoPanSpeed = server.arg("value").toInt();
      if (autoPanEnabled) {
        servo.write(servoPanSpeed);
      }
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/led", HTTP_GET, []() {
    if (server.hasArg("blue")) digitalWrite(LED_BLUE, server.arg("blue").toInt());
    if (server.hasArg("red")) digitalWrite(LED_RED, server.arg("red").toInt());
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/buzzer", HTTP_GET, []() {
    if (server.hasArg("state")) digitalWrite(BUZZERPIN, server.arg("state").toInt());
    server.send(200, "text/plain", "OK");
  });

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTML());
  });

  server.begin();
  Serial.println("API server started on port 80");
}

// ==================== Sensor Reading ====================
void readSensors() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  lightValue = analogRead(LDRPIN);
  rainDetected = (digitalRead(RAINPIN) == LOW);
}

void checkMotion() {
  bool motionNow = digitalRead(PIRPIN);
  unsigned long now = millis();
  if (motionNow && !motionDetected) {
    motionDetected = true;
    lastMotionTime = now;
    digitalWrite(LED_RED, HIGH);
    digitalWrite(BUZZERPIN, HIGH);
  } else if (!motionNow && motionDetected && (now - lastMotionTime > 5000)) {
    motionDetected = false;
    digitalWrite(LED_RED, LOW);
    digitalWrite(BUZZERPIN, LOW);
  }
}

// ==================== Servo Test ====================
void testServo() {
  Serial.println("Testing servo: moving left...");
  servo.write(70);
  delay(1000);
  Serial.println("Stop");
  servo.write(90);
  delay(500);
  Serial.println("Moving right...");
  servo.write(110);
  delay(1000);
  Serial.println("Stop");
  servo.write(90);
  delay(500);
  Serial.println("Servo test complete. Starting auto-pan.");
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nStarting FarmGuard Pro...");

  pinMode(DHTPIN, INPUT);
  pinMode(LDRPIN, INPUT);
  pinMode(RAINPIN, INPUT_PULLUP);
  pinMode(PIRPIN, INPUT);
  pinMode(BUZZERPIN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(BUZZERPIN, LOW);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_RED, LOW);

  servo.attach(SERVOPIN);
  servo.write(90);  // stop

  // Test servo to verify it's working
  testServo();

  // Start auto-pan with initial speed
  servo.write(servoPanSpeed);
  autoPanEnabled = true;

  dht.begin();
  initCamera();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  startCameraServer();
  setupAPI();

  digitalWrite(LED_BLUE, HIGH);
  Serial.println("System ready – open browser to http://" + WiFi.localIP().toString());
  Serial.println("Servo auto-pan active (speed adjustable via web)");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSensorRead > 2000) {
    lastSensorRead = now;
    readSensors();
  }
  checkMotion();
  server.handleClient();
  delay(10);
}