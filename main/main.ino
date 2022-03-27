#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>
#include <WiFiClient.h>
#include <SoftwareSerial.h>
#include "config.h"

ESP8266WiFiMulti WiFiMulti;

int rxPin = 0;
int txPin = 1;
SoftwareSerial esp(rxPin, txPin);

DHT dht(DHT_PIN, DHT_TYPE);

void setup() {

    Serial.begin(9600);
    esp.begin(9600);

    Serial.println();
    Serial.println();
    Serial.println();

    for (uint8_t t = 4; t > 0; t--) {
        Serial.printf("[SETUP] WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }

    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
    dht.begin();
}

void loop() {
    // wait for WiFi connection
    if ((WiFiMulti.run() == WL_CONNECTED)) {
        esp.println(WiFi.localIP());

        WiFiClient client;
        HTTPClient http;

        esp.print("[HTTP] begin esp...\n");
        if (http.begin(client, "http://localhost:8080/send/data")) {  // HTTP
            float hum = dht.readHumidity();
            float cels = dht.readTemperature();
            esp.println("Writing: cels = " + String(cels) + ", hum = " + String(hum));

            esp.print("[HTTP] POST esp...\n");
            // start connection and send HTTP header
            http.addHeader("Content-Type", "application/json");
            int httpCode = http.POST("{\"temperature\": \"" + (String) cels + "\",\"humidity\": \"" + (String) hum + "\"}");

            // httpCode will be negative on error
            if (httpCode > 0) {
                // HTTP header has been send and Server response header has been handled
                esp.printf("[HTTP] esp POST... code: %d\n", httpCode);
                // file found at server
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                    String payload = http.getString();
                    esp.println(payload);
                }
            } else {
                esp.printf("[HTTP] esp POST esp... failed, error: %s\n", http.errorToString(httpCode).c_str());
            }

            http.end();
        } else {
            esp.printf("[HTTP] esp Unable to connect\n");
        }
    }
    esp.println("Wait 5s before next round...");
    delay(5000);
}