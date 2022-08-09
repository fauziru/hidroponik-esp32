// Sistem Manajemen Mekanisasi Pertanian - Teknik Pertanian UNPAD
// NPM: 240110150116
// Code By Fauzi Reza Umara

// import library yang dibutuhkan
#include <ArduinoJson.h>
// WiFi
#include "WiFi.h"
#include <HTTPClient.h>
// TDS
#include <EEPROM.h>
#include <GravityTDS.h>
// Dallas
#include <OneWire.h>
#include <DallasTemperature.h>
// VL530X
#include <Wire.h>
#include <VL53L1X.h>
// MQTT Client
#include <PubSubClient.h>

// pin yang digunakan
// D33(adc) = Tds, D4 = Dallas,
// D21 = SDA, D22 = SCL Untuk vl53l1x

// define pin
#define TDSPIN 33
#define DALLASPIN 4

// inisiasi variabel
// WiFi
String id_node = "98e88cea-e998-4975-9281-da6a8de4aae1";
const char* hostname = "node-98e88cea-e998-4975-9281-da6a8de4aae1";
const char* ssid = "AlsinWifi_Ext";
const char* password = "WifiEXT22";
// mqtt
const char* mqttUsername = "dhydro";
const char* mqttPassword = "DhydrOFarm2022*";
//String path = "read/" + id_node;
// be server
String serverName = "https://api.hydrofarm.id/api/v1/";
String apiKey = "?secret=303T57EY0IKPBO5cnsGVLvrCZJZYOAoL";
const char path[] = "read/98e88cea-e998-4975-9281-da6a8de4aae1";
// tangki
float hTangki = 90;

// inisiasi class
GravityTDS gravityTds;
OneWire oneWire(DALLASPIN);
DallasTemperature dallas(&oneWire);
WiFiClient espClient;
IPAddress mqtt_server(5, 181, 217, 8);
PubSubClient mqttclient(mqtt_server, 1883, espClient);
VL53L1X vl53l1x;
HTTPClient http;

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
  // masukan formData
  int httpResponseCode = http.POST("opr=pwdLogin&userName=<>&pwd=<>&rememberPwd=0");

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}


void initMqtt() {
  if (mqttclient.connect(hostname, mqttUsername, mqttPassword)) {
    Serial.println("Koneksi ke mqtt berhasil...");
    mqttclient.publish("init", hostname);
  } else {
    Serial.println("Koneksi ke mqtt gagal...");
    Serial.println("Melakukan koneksi ulang...");
    reconnectMqtt();
  }
}

void reconnectMqtt() {
  initMqtt();
}

void initSensorTDS() {
  // Sensor TDS inittialization
  gravityTds.setPin(TDSPIN);
  gravityTds.setAref(3.3);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(4096.0);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTds.begin();  //initialization
}

void initSensorJarak() {
  Wire.begin();
  vl53l1x.setTimeout(500);
  if (!vl53l1x.init())
  {
    Serial.println("Failed to detect and initialize vl53l1x!");
    return;
  }

  // Use long distance mode and allow up to 50000 us (50 ms) for a measurement.
  // You can change these settings to adjust the performance of the sensor, but
  // the minimum timing budget is 20 ms for short distance mode and 33 ms for
  // medium and long distance modes. See the VL53L1X datasheet for more
  // information on range and timing limits.
  vl53l1x.setDistanceMode(VL53L1X::Long);
  vl53l1x.setMeasurementTimingBudget(50000);

  // Start continuous readings at a rate of one measurement every 50 ms (the
  // inter-measurement period). This period should be at least as long as the
  // timing budget.
  vl53l1x.startContinuous(50);
}
//############################
void setup() {
  Serial.begin(115200);
  Serial.println("Mulai ...");
  // begin dalas
  dallas.begin();
  initSensorTDS();
  initSensorJarak();
  initWiFi();
  loginPintas();
  initMqtt();
}

void loop() {
  const int capacity = JSON_OBJECT_SIZE(3);
  StaticJsonDocument<capacity> doc;
  float tempTdsValue = readDallas();
  float tdsValue = readTds(tempTdsValue);
  float dsValue = readVlx5311x();
  doc["tds"] = tdsValue;
  doc["tds_t"] = tempTdsValue;
  doc["height"] = dsValue; 
  String reads = "&read=" + doc.as<String>();
  Serial.println(reads);
  if (WiFi.status() == WL_CONNECTED) {
    //  post to server
    packToSendServer(reads);
    //  mqtt
    packToSendMqtt(tempTdsValue, tdsValue, dsValue);
  } else {
    Serial.println("WiFi Disconnected");
    // reconnect wifi
    initWiFi();
  }
  
  delay(1000);
}
//############################

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

void packToSendMqtt(float tempTds, float tdsValue, float dsValue) {
  if (mqttclient.connected()) {
    // urutan msgg humDht, tempDht, luxBH1750
    String tpc = "node/" + id_node;
    String topicTds = tpc + "/tds";
    String topicTempTds = tpc + "/tds_t";
    String topicDsitance = tpc + "/height";
    // String msg = String(tdsValue) + "," + String(tempTds) + "," + String(dhtVal.hum) + "," + String(dhtVal.temp) + "," + String(luxValue);

    //data logger per sensor
    mqttclient.publish(topicTds.c_str(), String(tdsValue).c_str());
    mqttclient.publish(topicTempTds.c_str(), String(tempTds).c_str());
    mqttclient.publish(topicDsitance.c_str(), String(dsValue).c_str());
  } else {
    // if fail 
    reconnectMqtt();
  }
}

float readTds(float temp) {
  // masukan hitungan di kalibrasi disini
  gravityTds.setKvalue(0.7);
  gravityTds.setTemperature(temp);  // set the temperature and execute temperature compensation
  gravityTds.update();  //sample and calculate
  float tdsValue = gravityTds.getTdsValue();  // then get the value
  float calibratedTds = 0;
  if(tdsValue > 0) {
    calibratedTds = 121.19 * pow(2.7182, (0.0053 * tdsValue));
  }
  float nilaiRawTds = analogRead(TDSPIN);
  float Volt = nilaiRawTds * (3.3 / 4095.0);
  Serial.print("Tegangan: ");
  Serial.print(Volt, 2);
  Serial.print("V ; Nilai raw TDS: ");
  Serial.print(nilaiRawTds);
  Serial.print("; Tds : ");
  Serial.print(tdsValue, 0);
  Serial.println("ppm");
  return calibratedTds;
}

float readDallas() {
  // masukan hitungan di kalibrasi disini
  // kirim command untuk mendapatkan nilai temperature
  dallas.requestTemperatures();
  float tempDalas = dallas.getTempCByIndex(0);
  float calibratedTempDallas = 13.498 * pow(2.7182, (0.0263 * tempDalas));
  // cek hasil pembacaan
  if (tempDalas != DEVICE_DISCONNECTED_C)
  {
    Serial.print("Temperature nutrisi: ");
    Serial.println(tempDalas);
  }
  else
  {
    Serial.println("Error: Could not read temperature data");
  }
  return tempDalas;
}

float readVlx5311x() {
  float val = vl53l1x.readRangeSingleMillimeters();
  float calibratedDis = (0.0002 * pow(val, 2) + (0.4276 * val) + 99.996) / 10;
  float hMukaAir = hTangki - calibratedDis;
  Serial.print("; Nilai raw vlx53310x: ");
  Serial.println(calibratedDis);
  Serial.print("; Tinggi muka air: ");
  Serial.println(hMukaAir);
  if (hMukaAir < 0) {
    return 0;
  }
  if (vl53l1x.timeoutOccurred()) { Serial.print(" TIMEOUT"); }
  return hMukaAir;
}
