#include <painlessMesh.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "Navira"
#define MESH_PASSWORD "Naviraselamanya"
#define MESH_PORT     5555

// Konfigurasi sensor DHT
#define DHTPIN 2
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// Identitas node
const char* nodeName = "Node-3(rasya)";

Scheduler userScheduler;
painlessMesh mesh;

// Prototipe fungsi pengiriman pesan
void sendMessage();

Task taskSendMessage(TASK_SECOND * 3, TASK_FOREVER, &sendMessage);

void sendMessage() {

  // Membaca suhu dan kelembapan
  float suhu = dht.readTemperature();
  float kelembapan = dht.readHumidity();

  // Cek pembacaan sensor
  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("[DHT Error] Gagal membaca data dari sensor DHT!");
    return;
  }

  // Membuat data JSON
  StaticJsonDocument<200> doc;

  doc["node"] = nodeName;
  doc["chipId"] = mesh.getNodeId();
  doc["suhu"] = suhu;
  doc["kelembapan"] = kelembapan;

  String msg;
  serializeJson(doc, msg);

  // Siarkan data ke seluruh jaringan mesh
  mesh.sendBroadcast(msg);

  Serial.print("[KIRIM MESH] ");
  Serial.println(msg);
}

// Callback saat menerima pesan
void receivedCallback(uint32_t from, String &msg) {

  StaticJsonDocument<200> doc;

  DeserializationError error = deserializeJson(doc, msg);

  if (!error) {

    const char* sender = doc["node"];
    uint32_t chipId = doc["chipId"];
    float suhu = doc["suhu"];
    float kelembapan = doc["kelembapan"];

    Serial.println("========================================");
    Serial.printf("[TERIMA DARI] %s (Node ID: %u | Chip ID: %u)\n",
                  sender, from, chipId);
    Serial.printf("Suhu       : %.2f °C\n", suhu);
    Serial.printf("Kelembapan : %.2f %%\n", kelembapan);
    Serial.println("========================================");

  } else {

    Serial.printf("[TERIMA DATA MENTAH DARI %u]: %s\n",
                  from, msg.c_str());
  }
}

// Callback saat ada node baru
void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> Koneksi Baru Terdeteksi! Node ID: %u\n", nodeId);
}

// Callback saat topologi berubah
void changedConnectionCallback() {
  Serial.println("--> Topologi rantai mesh telah diperbarui");
}

void setup() {

  Serial.begin(115200);

  // Mulai sensor DHT
  dht.begin();

  // Atur debug
  mesh.setDebugMsgTypes(ERROR | STARTUP);

  // Inisialisasi jaringan mesh
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);

  // Daftarkan callback
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  // Aktifkan pengiriman berkala
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();

  Serial.printf("Mesh Node [%s] Berjalan. Menunggu pembentukan topologi...\n",
                nodeName);
}

void loop() {

  mesh.update();
}