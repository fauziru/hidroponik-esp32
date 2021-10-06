/*
 *  Percobaan koneksi ke hostspot
 *  ref: https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/#3
 */

#include "WiFi.h"

// Hostname device
const char* hostname = "esp32-spot-1";
// Hotspot credential
const char* ssid = "Urf";
const char* password = "fauziru1234";

void initWiFi() {
  // WiFi initialization
  WiFi.mode(WIFI_STA);
  // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  initWiFi();
}

void loop() {
  // put your main code here, to run repeatedly:
  if(WiFi.status()== WL_CONNECTED){
    Serial.println("WiFi Connected");
  } else {
    Serial.println("WiFi Disconnected");
  }
}