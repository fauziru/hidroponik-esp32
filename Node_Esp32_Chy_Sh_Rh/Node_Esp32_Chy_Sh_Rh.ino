// Sistem Manajemen Mekanisasi Pertanian - Teknik Pertanian UNPAD
// NPM: 240110150116
// Code By Fauzi Reza Umara

// import library yang dibutuhkan
#include <ArduinoJson.h>
#include <math.h>
#include <pthread.h>
// WiFi
#include "WiFi.h"
//#include <HttpClient.h>
#include <HTTPClient.h>
// DHT
#include <Adafruit_Sensor.h>
#include <DHT.h>
// BH1750
#include <Wire.h>
#include <BH1750.h>
// MQTT Client
#include <PubSubClient.h>

// pin yang digunakan
// D5 = Dht
// D21 = SDA, D22 = SCL

// define
#define DHTTYPE DHT22
#define DHTPIN 5
#define SDA 21
#define SCL 22


// deklrasi variabel
// Wifi
String id_node = "9e33fa06-f5d3-446b-b0a7-64776a4cb74f";
const char* hostname = "node-9e33fa06-f5d3-446b-b0a7-64776a4cb74f";
const char* ssid = "AlsinWifi_Ext";
const char* password = "WifiEXT22";
// mqtt
const char* mqttUsername = "dhydro";
const char* mqttPassword = "DhydrOFarm2022*";
//const char* mqtt_server = "103.156.114.74";
//String path = "read/" + id_node;
// be server
String apiKey = "?secret=303T57EY0IKPBO5cnsGVLvrCZJZYOAoL";
String serverName = "https://api.hydrofarm.id/api/v1/";

//const char serverName[] = "http://127.0.0.1:8080/api/v1/";
const char path[] = "read/9e33fa06-f5d3-446b-b0a7-64776a4cb74f";

// inisiasi class
//GravityTDS gravityTds;
DHT dht(DHTPIN, DHTTYPE);
//OneWire oneWire(DALLASPIN);
//DallasTemperature dallas(&oneWire);
BH1750 lightMeter(0x23);
WiFiClient espClient;
IPAddress mqtt_server(5, 181, 217, 8); // ip mqtt
PubSubClient mqttclient(mqtt_server, 1883, espClient); // (IP, Port, Client)
HTTPClient http;


// struct
struct DhtTipe {
  float hum;
  float temp;
};

// init function
void initBH1750() {
  Wire.begin(SDA, SCL); // SDA,SCL
  lightMeter.begin();
  delay(1000);
}

void initWiFi() {
  // WiFi initialization
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  Serial.println("Menyambung koneksi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Terkoneksi dengan IP: ");
  Serial.println(WiFi.localIP());
  //  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");
}

void loginPintas() {
  http.begin("http://pintas.unpad.ac.id/ac_portal/login.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // masukan x-www-formurlencoded
  int httpResponseCode = http.POST("opr=pwdLogin&userName=<>&pwd=<>&rememberPwd=0");

  if (httpResponseCode == 200) {
    Serial.print("Login berhasil; HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
  } else {
    Serial.print("Login gagal; Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

void initMqtt() {
  if (mqttclient.connect(hostname, mqttUsername, mqttPassword)) {
    Serial.println("Koneksi ke mqtt berhasil...");
    String initTopic = "init/" + id_node;
    mqttclient.publish(initTopic.c_str(), hostname);
  } else {
    Serial.println("Koneksi ke mqtt gagal...");
    Serial.println("Melakukan koneksi ulang...");
    reconnectMqtt();
  }
}

void reconnectMqtt() {
  delay(200);
  initMqtt();
}
//############################{
void setup() {
  Serial.begin(115200);
  Serial.println("Mulai ...");
  initWiFi();
  loginPintas();
//   begin dht class
  dht.begin();
//   begin lux class
  initBH1750();
  initMqtt();
}

void loop() {
  const int capacity = JSON_OBJECT_SIZE(3);
  StaticJsonDocument<capacity> doc;
  DhtTipe dhtValue = readDht();
  float luxValue = readBH1750();
  // {"hum": 102, "temp": 10, "lux": 22}
  doc["hum"] = ceil(dhtValue.hum * 100) / 100;
  doc["temp"] = ceil(dhtValue.temp * 100) / 100;
  doc["light"] = ceil(luxValue * 100) / 100;
  String reads = "&read=" + doc.as<String>();
  Serial.println(reads);
  
//print_dht(dhtValue);
//Serial.println(luxValue);
//cek koneksi wifi, jika ada kirim, selain itu koneksi ulang
  if (WiFi.status() == WL_CONNECTED) {
//    post to server
    packToSendServer(reads);
//    mqtt
    packToSendMqtt(dhtValue, luxValue);
  } else {
    Serial.println("WiFi Disconnected");
//  reconnect wifi
    initWiFi();
  }

  delay(1000);
}
//############################}
void packToSendServer(String reads) {
  long int t1 = millis();
  HTTPClient http;
  String endpoint = serverName + path + apiKey + reads;
  http.begin(endpoint.c_str());
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    long int t2 = millis();
    Serial.print("Request time: ");
    Serial.print(t2-t1);
    Serial.println(" milliseconds");
    Serial.println(payload);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}
void packToSendMqtt(const DhtTipe &dhtVal, float luxValue) {
  if (mqttclient.connected()) {
    // urutan msgg humDht, tempDht, luxBH1750
    String tpc = "node/" + id_node;
    String topicHum = tpc + "/hum";
    String topicTemp = tpc + "/temp";
    String topicLux = tpc + "/light";
    // String msg = String(tdsValue) + "," + String(tempTds) + "," + String(dhtVal.hum) + "," + String(dhtVal.temp) + "," + String(luxValue);

    //data logger per sensor
    mqttclient.publish(topicHum.c_str(), String(dhtVal.hum).c_str());
    mqttclient.publish(topicTemp.c_str(), String(dhtVal.temp).c_str());
    mqttclient.publish(topicLux.c_str(), String(luxValue).c_str());
  } else {
    reconnectMqtt();
  }
}

DhtTipe readDht() {
  // perhitungan kalibrasi dan ambil data Dht
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float calibratedHum = 0.0303 * pow(h, 2) - (3.4369 * h) + 137.83;
  float calibratedTemp = 0.0583 * pow(t, 2) - (1.7306 * t) + 31.359;
  Serial.print(F("Humidity lingkungan: "));
  Serial.print(h);
  Serial.print(F("%  Temperature lingkungan: "));
  Serial.print(t);
  Serial.println(F("°C "));
  DhtTipe result = {calibratedHum, calibratedTemp};
  return result;
}

void print_dht(const DhtTipe &dhtVal) {
  Serial.print(dhtVal.hum);
  Serial.println(dhtVal.temp);
}

float readBH1750() {
  // masukan hitungan di kalibrasi disini
  uint16_t lux = lightMeter.readLightLevel();
  float calibratedLux = (2.1819 * lux);
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");
  return calibratedLux;
}
