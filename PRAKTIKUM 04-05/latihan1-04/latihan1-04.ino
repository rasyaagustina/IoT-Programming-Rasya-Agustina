#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

const char* ssid = "cacicul";
const char* password = "pakeajaa";

// Konfigurasi Pin
const byte dhtPin = 2;        // D4 (GPIO 2)
const byte buttonPin = 4;     // D2 (GPIO 4)
const byte ledPin = 12;       // D6 (GPIO 12)

DHT dht(dhtPin, DHT11);

// Variabel Pelacak Status
bool ledState = false;

String currentTemp = "--";
String currentHum = "--";

int buttonState;
int lastButtonState = LOW;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long lastTime = 0;

// Inisialisasi Web Server dan WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// =====================================================
// HTML & JAVASCRIPT
// =====================================================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>

<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Real-Time IoT Web</title>

  <style>
    body {
      font-family: Arial;
      text-align: center;
    }

    .card {
      background: #f0f0f0;
      margin: 20px auto;
      padding: 20px;
      max-width: 300px;
      border-radius: 10px;
    }

    button {
      padding: 15px 30px;
      font-size: 20px;
      border-radius: 5px;
      cursor: pointer;
      color: white;
    }

    .btn-on {
      background-color: #4CAF50;
    }

    .btn-off {
      background-color: #f44336;
    }
  </style>
</head>

<body>

  <h1>Smart Room</h1>

  <!-- Tampilan Suhu -->
  <div class="card">
    <h2>Suhu: <span id="tempValue">--</span> Celcius</h2>
  </div>

  <!-- Tampilan Kelembapan -->
  <div class="card">
    <h2>Kelembapan: <span id="humValue">--</span> %</h2>
  </div>

  <!-- Tampilan LED -->
  <div class="card">
    <h2>LED: <span id="ledStatus">OFF</span></h2>

    <button
      id="toggleBtn"
      class="btn-off"
      onclick="toggleLed()">
      Turn ON
    </button>
  </div>

  <script>

    // Membuka koneksi WebSocket
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    // Saat halaman pertama kali dibuka
    window.addEventListener('load', onLoad);

    function onLoad(event) {
      initWebSocket();
    }

    // Inisialisasi WebSocket
    function initWebSocket() {

      websocket = new WebSocket(gateway);

      websocket.onopen = onOpen;
      websocket.onclose = onClose;
      websocket.onmessage = onMessage;
    }

    // Saat WebSocket berhasil terhubung
    function onOpen(event) {
      console.log('WebSocket Terkoneksi');
    }

    // Jika WebSocket terputus
    function onClose(event) {
      setTimeout(initWebSocket, 2000);
    }

    // Tombol LED pada halaman web
    function toggleLed() {
      websocket.send('toggle');
    }

    // Menerima data JSON dari NodeMCU
    function onMessage(event) {

      var dataObj = JSON.parse(event.data);

      // Menampilkan suhu
      if (dataObj.suhu !== undefined) {
        document.getElementById('tempValue').innerHTML =
          dataObj.suhu;
      }

      // Menampilkan kelembapan
      if (dataObj.hum !== undefined) {
        document.getElementById('humValue').innerHTML =
          dataObj.hum;
      }

      // Mengubah tampilan status LED
      if (dataObj.led !== undefined) {

        var btn = document.getElementById('toggleBtn');
        var status = document.getElementById('ledStatus');

        if (dataObj.led == "1") {

          status.innerHTML = "ON";
          btn.innerHTML = "Turn OFF";
          btn.className = "btn-on";

        } else {

          status.innerHTML = "OFF";
          btn.innerHTML = "Turn ON";
          btn.className = "btn-off";
        }
      }
    }

  </script>

</body>
</html>
)rawliteral";

// =====================================================
// BACK-END & WEBSOCKET
// =====================================================

// Mengirim data ke seluruh browser
void notifyClients() {

  String jsonString =
    "{\"led\":\"" +
    String(ledState ? 1 : 0) +
    "\", ";

  jsonString +=
    "\"suhu\":\"" +
    currentTemp +
    "\", ";

  jsonString +=
    "\"hum\":\"" +
    currentHum +
    "\"}";

  ws.textAll(jsonString);
}

// Menerima pesan dari browser
void handleWebSocketMessage(
  void *arg,
  uint8_t *data,
  size_t len
) {

  AwsFrameInfo *info =
    (AwsFrameInfo*)arg;

  if (
    info->final &&
    info->index == 0 &&
    info->len == len &&
    info->opcode == WS_TEXT
  ) {

    data[len] = 0;

    if (strcmp((char*)data, "toggle") == 0) {

      ledState = !ledState;

      notifyClients();
    }
  }
}

// Event WebSocket
void onEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
) {

  switch (type) {

    case WS_EVT_CONNECT:

      Serial.printf(
        "Client WebSocket #%u terhubung\n",
        client->id()
      );

      notifyClients();

      break;

    case WS_EVT_DISCONNECT:

      Serial.printf(
        "Client WebSocket #%u terputus\n",
        client->id()
      );

      break;

    case WS_EVT_DATA:

      handleWebSocketMessage(
        arg,
        data,
        len
      );

      break;
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // Tombol menggunakan pull-down eksternal
  pinMode(buttonPin, INPUT);

  // LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // DHT
  dht.begin();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println(
    "\nIP Address: " +
    WiFi.localIP().toString()
  );

  // Web Server
  server.on(
    "/",
    HTTP_GET,
    [](AsyncWebServerRequest *request) {

      request->send_P(
        200,
        "text/html",
        index_html
      );
    }
  );

  // WebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.begin();
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // Membersihkan client WebSocket yang terputus
  ws.cleanupClients();

  // Mengatur kondisi LED
  digitalWrite(
    ledPin,
    ledState ? HIGH : LOW
  );

  // Membaca tombol fisik
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {

    lastDebounceTime = millis();
  }

  if (
    (millis() - lastDebounceTime)
    > debounceDelay
  ) {

    if (reading != buttonState) {

      buttonState = reading;

      // Tombol ditekan = HIGH
      if (buttonState == HIGH) {

        ledState = !ledState;

        notifyClients();
      }
    }
  }

  lastButtonState = reading;

  // Membaca DHT setiap 3 detik
  if ((millis() - lastTime) > 3000) {

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    // Memastikan data suhu valid
    if (!isnan(t)) {

      currentTemp = String(t);
    }

    // Memastikan data kelembapan valid
    if (!isnan(h)) {

      currentHum = String(h);
    }

    // Mengirim suhu dan kelembapan
    // ke seluruh browser melalui WebSocket
    notifyClients();

    lastTime = millis();
  }
}