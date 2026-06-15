// ============================================================
//  Sistema de Monitoramento de Frota — Dispositivo ESP32
//  Wokwi Simulator
// ============================================================
//  Dependências (libraries no Wokwi):
//    - PubSubClient by Nick O'Leary
//    - ArduinoJson by Benoit Blanchon
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ------------------------------------------------------------
// CONFIGURAÇÃO — altere aqui para cada veículo simulado
// ------------------------------------------------------------
#define VEHICLE_ID     "veiculo01"  //Altere para veiculo02, veiculo03, etc. para simular mais unidades
#define EMPRESA        "empresa"    // Nome da empresa (pode ser qualquer string, usada nos tópicos MQTT)

// Coordenadas iniciais (lat/lng fictícias — Uberlândia, MG) 
// Altere conforme fazer outras unidades para que comecem em locais diferentes. 
#define START_LAT  -18.9186 
#define START_LNG  -48.2772

#define PIN_LED_ONLINE 2
#define PIN_LED_PARADO 4

// Broker HiveMQ Cloud
const char* MQTT_HOST     = "468858741cce418ea6219352a279b8f5.s1.eu.hivemq.cloud";
const char* MQTT_USER     = "mqttbroker";            
const char* MQTT_PASSWORD = "Zz7LTTuA";              
const int   MQTT_PORT     = 8883;

// WiFi (Wokwi usa rede simulada — qualquer SSID/senha funciona)
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// Intervalo de publicação de posição (ms)
const unsigned long PUBLISH_INTERVAL = 3000;

// ------------------------------------------------------------
// Tópicos MQTT
// ------------------------------------------------------------
// Monta os tópicos dinamicamente a partir de EMPRESA e VEHICLE_ID
char TOPIC_POSICAO[64];   // frota/empresa/veiculoXX/posicao
char TOPIC_STATUS[64];    // frota/empresa/veiculoXX/status
char TOPIC_COMANDO[64];   // frota/empresa/veiculoXX/comando
char CLIENT_ID[32];       // mqtt-veiculoXX

// ------------------------------------------------------------
// Simulação de rota
// ------------------------------------------------------------
// Cada veículo percorre uma rota com N waypoints (lat/lng).
// O sketch interpola linearmente entre eles para gerar movimento.
// Altere conforme fazer outras unidades para criar rotas diferentes.
const int NUM_WAYPOINTS = 6;
float routeLat[NUM_WAYPOINTS] = {
  -18.9186, -18.9120, -18.9050, -18.9100, -18.9200, -18.9186
};
float routeLng[NUM_WAYPOINTS] = {
  -48.2772, -48.2700, -48.2620, -48.2500, -48.2560, -48.2772
};

int   currentWaypoint  = 0;
float currentLat       = 0;
float currentLng       = 0;
float speed_kmh        = 0;   // calculada na interpolação
bool  vehicleStopped   = false;

// Controla quantos passos de interpolação entre waypoints
const int STEPS_BETWEEN = 20;
int currentStep = 0;

// ------------------------------------------------------------
// Estado do veículo
// ------------------------------------------------------------
int   batteryLevel    = 100;  // percentual fictício
bool  engineOn        = true;

// ------------------------------------------------------------
// Objetos MQTT / WiFi
// ------------------------------------------------------------
WiFiClientSecure wifiClient;
PubSubClient     mqttClient(wifiClient);

unsigned long lastPublishTime = 0;

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED_ONLINE, OUTPUT);
  pinMode(PIN_LED_PARADO, OUTPUT);
  digitalWrite(PIN_LED_ONLINE, HIGH); // acende verde ao conectar


  // Monta os tópicos e client ID
  snprintf(TOPIC_POSICAO, sizeof(TOPIC_POSICAO), "frota/%s/%s/posicao", EMPRESA, VEHICLE_ID);
  snprintf(TOPIC_STATUS,  sizeof(TOPIC_STATUS),  "frota/%s/%s/status",  EMPRESA, VEHICLE_ID);
  snprintf(TOPIC_COMANDO, sizeof(TOPIC_COMANDO), "frota/%s/%s/comando", EMPRESA, VEHICLE_ID);
  snprintf(CLIENT_ID,     sizeof(CLIENT_ID),     "mqtt-%s",             VEHICLE_ID);

  Serial.printf("\n=== Frota ESP32 | %s ===\n", VEHICLE_ID);
  Serial.printf("Tópico posição : %s\n", TOPIC_POSICAO);
  Serial.printf("Tópico status  : %s\n", TOPIC_STATUS);
  Serial.printf("Tópico comando : %s\n", TOPIC_COMANDO);

  // Posição inicial
  currentLat = routeLat[0];
  currentLng = routeLng[0];

  connectWiFi();

  // HiveMQ Cloud exige TLS — no Wokwi usamos setInsecure() pois
  // o simulador não tem repositório de CA raiz confiável.
  // Em hardware físico, use setCACert() com o certificado ISRG Root X1.
  wifiClient.setInsecure();

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(onMqttMessage);
  mqttClient.setBufferSize(512);

  connectMQTT();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // Reconecta se necessário
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  // Publica posição periodicamente
  unsigned long now = millis();
  if (now - lastPublishTime >= PUBLISH_INTERVAL) {
    lastPublishTime = now;

    if (!vehicleStopped) {
      updatePosition();
    }

    publishPosition();
    simulateBattery();
  }
}

// ============================================================
//  CONEXÃO WiFi
// ============================================================
void connectWiFi() {
  Serial.printf("Conectando ao WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi OK — IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nFalha no WiFi! Reiniciando...");
    ESP.restart();
  }
}

// ============================================================
//  CONEXÃO MQTT (com LWT e retained no status)
// ============================================================
void connectMQTT() {
  Serial.printf("Conectando ao broker MQTT (%s)...\n", MQTT_HOST);

  // Monta payload do LWT (Last Will and Testament)
  // Se o ESP32 cair sem avisar, o broker publica este payload automaticamente.
  StaticJsonDocument<128> lwtDoc;
  lwtDoc["veiculo"]  = VEHICLE_ID;
  lwtDoc["status"]   = "offline";
  lwtDoc["motivo"]   = "LWT";

  char lwtPayload[128];
  serializeJson(lwtDoc, lwtPayload);

  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    // connect(clientId, user, pass, lwtTopic, lwtQos, lwtRetain, lwtMessage)
    bool ok = mqttClient.connect(
      CLIENT_ID,
      MQTT_USER,
      MQTT_PASSWORD,
      TOPIC_STATUS,   // tópico do LWT
      1,              // QoS 1 para o LWT
      true,           // retained = true → dashboard sempre vê o último status
      lwtPayload
    );

    if (ok) {
      Serial.println("MQTT conectado!");

      // Publica status "online" com retained = true
      publishStatus("online");

      // Assina o tópico de comandos com QoS 2
      // (PARAR / BUZINAR não podem ser perdidos)
      mqttClient.subscribe(TOPIC_COMANDO, 1);
      Serial.printf("Subscrito em: %s (QoS 1)\n", TOPIC_COMANDO);

    } else {
      Serial.printf("Falha MQTT, código: %d. Tentativa %d/5\n",
                    mqttClient.state(), attempts + 1);
      delay(3000);
      attempts++;
    }
  }
}

// ============================================================
//  CALLBACK — recebe comandos do dashboard
// ============================================================
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.printf("\n[COMANDO] Tópico: %s\n", topic);
  Serial.printf("[COMANDO] Payload: %s\n", message);

  // Comandos aceitos: "PARAR", "RETOMAR", "BUZINAR"
  if (strcmp(message, "PARAR") == 0) {
    digitalWrite(PIN_LED_PARADO, HIGH);
    vehicleStopped = true;
    Serial.println(">>> Veículo PARADO por comando remoto!");
    publishStatus("parado");

  } else if (strcmp(message, "RETOMAR") == 0) {
    digitalWrite(PIN_LED_PARADO, LOW);
    vehicleStopped = false;
    Serial.println(">>> Veículo RETOMANDO rota!");
    publishStatus("em_rota");

  } else if (strcmp(message, "BUZINAR") == 0) {
    Serial.println(">>> BUZINANDO! *beep beep*");
    // No hardware real: acionar GPIO do buzzer aqui
  }
}

// ============================================================
//  ATUALIZA POSIÇÃO (interpolação entre waypoints)
// ============================================================
void updatePosition() {
  int nextWaypoint = (currentWaypoint + 1) % NUM_WAYPOINTS;

  float targetLat = routeLat[nextWaypoint];
  float targetLng = routeLng[nextWaypoint];

  // Interpolação linear
  float t = (float)(currentStep + 1) / STEPS_BETWEEN;
  float prevLat = currentLat;
  float prevLng = currentLng;

  currentLat = routeLat[currentWaypoint] + t * (targetLat - routeLat[currentWaypoint]);
  currentLng = routeLng[currentWaypoint] + t * (targetLng - routeLng[currentWaypoint]);

  // Estima velocidade pelo deslocamento (simplificado)
  float dLat = currentLat - prevLat;
  float dLng = currentLng - prevLng;
  float dist  = sqrt(dLat * dLat + dLng * dLng) * 111000; // aprox metros
  speed_kmh   = (dist / (PUBLISH_INTERVAL / 1000.0)) * 3.6;

  currentStep++;
  if (currentStep >= STEPS_BETWEEN) {
    currentStep    = 0;
    currentWaypoint = nextWaypoint;
    Serial.printf("Waypoint %d alcançado\n", currentWaypoint);
  }
}

// ============================================================
//  PUBLICA POSIÇÃO (QoS 1, retained)
// ============================================================
void publishPosition() {
  int velocidadePublicada = vehicleStopped ? 0 : (int)speed_kmh;
  StaticJsonDocument<256> doc;
  doc["veiculo"]   = VEHICLE_ID;
  doc["lat"]       = serialized(String(currentLat, 6));
  doc["lng"]       = serialized(String(currentLng, 6));
  doc["velocidade"]= velocidadePublicada;
  doc["bateria"]   = batteryLevel;
  doc["parado"]    = vehicleStopped;
  doc["ts"]        = millis();  // timestamp relativo

  char payload[256];
  serializeJson(doc, payload);

  // QoS 1 + retained: dashboard sempre tem a última posição conhecida
  bool ok = mqttClient.publish(TOPIC_POSICAO, payload, true);  // true = retained
  
  digitalWrite(PIN_LED_ONLINE, LOW);
  delay(80);
  digitalWrite(PIN_LED_ONLINE, HIGH);

  Serial.printf("[PUB] %s → %s | vel: %d km/h | bat: %d%%\n",
    TOPIC_POSICAO, payload, (int)speed_kmh, batteryLevel);

  if (!ok) {
    Serial.println("Falha ao publicar! Buffer cheio ou desconectado.");
  }
}

// ============================================================
//  PUBLICA STATUS (QoS 1, retained)
// ============================================================
void publishStatus(const char* statusStr) {
  StaticJsonDocument<128> doc;
  doc["veiculo"] = VEHICLE_ID;
  doc["status"]  = statusStr;
  doc["bateria"] = batteryLevel;

  char payload[128];
  serializeJson(doc, payload);

  // retained = true → quem conectar depois vê o último status
  mqttClient.publish(TOPIC_STATUS, payload, true);
  Serial.printf("[STATUS] %s → %s\n", TOPIC_STATUS, statusStr);
}

// ============================================================
//  SIMULA DESCARGA DE BATERIA
// ============================================================
void simulateBattery() {
  // Descarrega ~1% a cada 30 publicações (~90 segundos)
  static int publishCount = 0;
  publishCount++;
  if (publishCount % 30 == 0 && batteryLevel > 5) {
    batteryLevel--;
  }
}
