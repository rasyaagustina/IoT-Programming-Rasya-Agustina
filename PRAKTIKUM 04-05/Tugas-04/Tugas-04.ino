#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

// =====================================================
// KONFIGURASI WIFI
// =====================================================

const char* ssid = "cacicul";
const char* password = "pakeajaa";

// =====================================================
// KONFIGURASI PIN
// =====================================================

const byte dhtPin = 2;          // D4 / GPIO2
const byte buttonPin = 4;       // D2 / GPIO4
const byte ledPin = 12;         // D6 / GPIO12 - LED ON/OFF
const byte pwmLedPin = 5;       // D1 / GPIO5 - LED PWM

DHT dht(dhtPin, DHT22);

// =====================================================
// VARIABEL
// =====================================================

bool ledState = false;

String currentTemp = "--";
String currentHum = "--";

int pwmValue = 0;

int buttonState = LOW;
int lastButtonState = LOW;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long lastTime = 0;

// =====================================================
// WEB SERVER DAN WEBSOCKET
// =====================================================

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// =====================================================
// HTML + CSS + JAVASCRIPT
// =====================================================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

  <meta name="viewport"
        content="width=device-width, initial-scale=1">

  <title>Smart Lighting WebSocket</title>

  <style>

    body {
      font-family: Arial;
      text-align: center;
      background: #ffffff;
    }

    .card {
      background: #f0f0f0;
      margin: 20px auto;
      padding: 20px;
      max-width: 350px;
      border-radius: 10px;
    }

    button {
      padding: 15px 30px;
      font-size: 20px;
      border-radius: 5px;
      cursor: pointer;
      color: white;
      border: none;
    }

    .btn-on {
      background-color: #4CAF50;
    }

    .btn-off {
      background-color: #f44336;
    }

    input[type=range] {
      width: 90%;
    }

    .value {
      font-size: 24px;
      font-weight: bold;
    }

  </style>

</head>

<body>

  <h1>Smart Room</h1>

  <!-- ========================================= -->
  <!-- SUHU -->
  <!-- ========================================= -->

  <div class="card">

    <h2>
      Suhu:
      <span id="tempValue">--</span>
      Celcius
    </h2>

  </div>

  <!-- ========================================= -->
  <!-- KELEMBAPAN -->
  <!-- ========================================= -->

  <div class="card">

    <h2>
      Kelembapan:
      <span id="humValue">--</span>
      %
    </h2>

  </div>

  <!-- ========================================= -->
  <!-- LED D6 ON / OFF -->
  <!-- ========================================= -->

  <div class="card">

    <h2>
      LED:
      <span id="ledStatus">OFF</span>
    </h2>

    <button
      id="toggleBtn"
      class="btn-off"
      onclick="toggleLed()">

      Turn ON

    </button>

  </div>

  <!-- ========================================= -->
  <!-- PWM SLIDER -->
  <!-- ========================================= -->

  <div class="card">

    <h2>Kontrol Intensitas Cahaya</h2>

    <p>
      Nilai PWM:
      <span id="pwmValue" class="value">0</span>
    </p>

    <input
      type="range"
      min="0"
      max="1023"
      value="0"
      id="pwmSlider"
      oninput="sendPWM(this.value)">

  </div>

  <script>

    // =========================================
    // WEBSOCKET
    // =========================================

    var gateway =
      `ws://${window.location.hostname}/ws`;

    var websocket;

    window.addEventListener(
      'load',
      onLoad
    );

    function onLoad(event) {

      initWebSocket();

    }

    function initWebSocket() {

      websocket =
        new WebSocket(gateway);

      websocket.onopen =
        onOpen;

      websocket.onclose =
        onClose;

      websocket.onmessage =
        onMessage;

    }

    function onOpen(event) {

      console.log(
        'WebSocket Terkoneksi'
      );

    }

    function onClose(event) {

      console.log(
        'WebSocket Terputus'
      );

      setTimeout(
        initWebSocket,
        2000
      );

    }

    // =========================================
    // KONTROL LED ON / OFF
    // =========================================

    function toggleLed() {

      websocket.send(
        'toggle'
      );

    }

    // =========================================
    // KONTROL PWM
    // =========================================

    function sendPWM(value) {

      document.getElementById(
        'pwmValue'
      ).innerHTML = value;

      websocket.send(
        'pwm,' + value
      );

    }

    // =========================================
    // MENERIMA DATA DARI NODEMCU
    // =========================================

    function onMessage(event) {

      var dataObj =
        JSON.parse(event.data);

      // -------------------------------
      // SUHU
      // -------------------------------

      if (
        dataObj.suhu !== undefined
      ) {

        document.getElementById(
          'tempValue'
        ).innerHTML =
          dataObj.suhu;

      }

      // -------------------------------
      // KELEMBAPAN
      // -------------------------------

      if (
        dataObj.hum !== undefined
      ) {

        document.getElementById(
          'humValue'
        ).innerHTML =
          dataObj.hum;

      }

      // -------------------------------
      // LED ON / OFF
      // -------------------------------

      if (
        dataObj.led !== undefined
      ) {

        var btn =
          document.getElementById(
            'toggleBtn'
          );

        var status =
          document.getElementById(
            'ledStatus'
          );

        if (
          dataObj.led == "1"
        ) {

          status.innerHTML =
            "ON";

          btn.innerHTML =
            "Turn OFF";

          btn.className =
            "btn-on";

        }

        else {

          status.innerHTML =
            "OFF";

          btn.innerHTML =
            "Turn ON";

          btn.className =
            "btn-off";

        }

      }

      // -------------------------------
      // PWM
      // -------------------------------

      if (
        dataObj.pwm !== undefined
      ) {

        document.getElementById(
          'pwmValue'
        ).innerHTML =
          dataObj.pwm;

        document.getElementById(
          'pwmSlider'
        ).value =
          dataObj.pwm;

      }

    }

  </script>

</body>

</html>
)rawliteral";

// =====================================================
// MENGIRIM DATA KE SEMUA CLIENT
// =====================================================

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
    "\", ";

  jsonString +=
    "\"pwm\":\"" +
    String(pwmValue) +
    "\"}";

  ws.textAll(
    jsonString
  );

}

// =====================================================
// MENERIMA PESAN WEBSOCKET
// =====================================================

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

    String message =
      String((char*)data);

    // =========================================
    // KONTROL LED ON / OFF
    // =========================================

    if (
      message == "toggle"
    ) {

      ledState =
        !ledState;

      notifyClients();

    }

    // =========================================
    // KONTROL PWM
    // Format: pwm,512
    // =========================================

    else if (
      message.startsWith("pwm,")
    ) {

      String valueString =
        message.substring(4);

      int value =
        valueString.toInt();

      // Membatasi nilai PWM
      // dari 0 sampai 1023

      value =
        constrain(
          value,
          0,
          1023
        );

      pwmValue =
        value;

      analogWrite(
        pwmLedPin,
        pwmValue
      );

      notifyClients();

    }

  }

}

// =====================================================
// EVENT WEBSOCKET
// =====================================================

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

  Serial.begin(
    115200
  );

  // -------------------------------
  // PIN TOMBOL
  // -------------------------------

  pinMode(
    buttonPin,
    INPUT
  );

  // -------------------------------
  // LED ON / OFF D6
  // -------------------------------

  pinMode(
    ledPin,
    OUTPUT
  );

  digitalWrite(
    ledPin,
    LOW
  );

  // -------------------------------
  // LED PWM D1
  // -------------------------------

  pinMode(
    pwmLedPin,
    OUTPUT
  );

  analogWrite(
    pwmLedPin,
    0
  );

  // -------------------------------
  // SENSOR DHT
  // -------------------------------

  dht.begin();

  // -------------------------------
  // WIFI
  // -------------------------------

  WiFi.mode(
    WIFI_STA
  );

  WiFi.begin(
    ssid,
    password
  );

  Serial.print(
    "Menghubungkan WiFi"
  );

  while (
    WiFi.status() != WL_CONNECTED
  ) {

    delay(500);

    Serial.print(
      "."
    );

  }

  Serial.println();

  Serial.println(
    "WiFi Terhubung"
  );

  Serial.println(
    "IP Address: " +
    WiFi.localIP().toString()
  );

  // -------------------------------
  // WEB SERVER
  // -------------------------------

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

  // -------------------------------
  // WEBSOCKET
  // -------------------------------

  ws.onEvent(
    onEvent
  );

  server.addHandler(
    &ws
  );

  server.begin();

  Serial.println(
    "Web Server dimulai"
  );

}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // Membersihkan client WebSocket
  ws.cleanupClients();

  // =========================================
  // LED D6 ON / OFF
  // =========================================

  digitalWrite(
    ledPin,
    ledState ? HIGH : LOW
  );

  // =========================================
  // MEMBACA PUSH BUTTON
  // =========================================

  int reading =
    digitalRead(
      buttonPin
    );

  if (
    reading != lastButtonState
  ) {

    lastDebounceTime =
      millis();

  }

  if (
    (millis() - lastDebounceTime)
    > debounceDelay
  ) {

    if (
      reading != buttonState
    ) {

      buttonState =
        reading;

      // Tombol ditekan = HIGH

      if (
        buttonState == HIGH
      ) {

        ledState =
          !ledState;

        notifyClients();

      }

    }

  }

  lastButtonState =
    reading;

  // =========================================
  // MEMBACA DHT SETIAP 3 DETIK
  // =========================================

  if (
    (millis() - lastTime)
    > 3000
  ) {

    float t =
      dht.readTemperature();

    float h =
      dht.readHumidity();

    if (
      !isnan(t)
    ) {

      currentTemp =
        String(t);

    }

    if (
      !isnan(h)
    ) {

      currentHum =
        String(h);

    }

    // Kirim data ke browser
    notifyClients();

    lastTime =
      millis();

  }

}