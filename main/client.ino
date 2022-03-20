#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include <DHT.h>
#include "config.h" // Configuration file
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include <SoftwareSerial.h>

#include <WiFiClientSecureBearSSL.h>
// Fingerprint for demo URL, expires on June 2, 2021, needs to be updated well before this date
// const uint8_t fingerprint[20] = {0x40, 0xaf, 0x00, 0x6b, 0xec, 0x90, 0x22, 0x41, 0x8e, 0xa3, 0xad, 0xfa, 0x1a, 0xe8, 0x25, 0x41, 0x1d, 0x1a, 0x54, 0xb3};

ESP8266WiFiMulti wifiMulti;
WiFiClient wifiClient;

int rxPin = 0;
int txPin = 1;
SoftwareSerial esp(rxPin, txPin);

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient influxDbClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// Data point
Point sensor("arduino");
// Influx Clients
HTTPClient httpInflux;
// Sensor
DHT dht(DHT_PIN, DHT_TYPE);

void setup() {

    Serial.begin(115200);
    Serial.setTimeout(2000);
    esp.begin(9600);
    // Serial.setDebugOutput(true);

    esp.println();
    // Wait for serial to initialize.
    while(!Serial) { }

    for (uint8_t t = 4; t > 0; t--) {
        esp.printf("[SETUP] WAIT %d...\n", t);
        esp.flush();
        delay(1000);
    }

    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
    esp.println("Connected to SSID: " + (String) WIFI_SSID);

    pinMode(DHT_PIN, INPUT);
    pinMode(D0, OUTPUT);
    digitalWrite(D0, HIGH);
    dht.begin();
    timer.setInterval(5000L, getdata);

    esp.println("Device Started");
    esp.println("-------------------------------------");
    esp.println("Running DHT!");
    esp.println("-------------------------------------");
}

void loop() {
    // wait for WiFi connection
    //  if ((wifiMulti.run() == WL_CONNECTED)) {

    //std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

    // client->setFingerprint(fingerprint);
    // Or, if you happy to ignore the SSL certificate, then use the following line instead:
    //       client->setInsecure();

//        HTTPClient https;
//
//        sensor.addTag("device", DEVICE);
//        sensor.addTag("SSID", WiFi.SSID());
//        timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

    esp.print("[HTTPS] begin...\n");

//        // Check server connection
//        if (influxDbClient.validateConnection()) {
//            esp.print("Connected to InfluxDB: ");
//            esp.println(influxDbClient.getServerUrl());
//        } else {
//            esp.print("InfluxDB connection failed: ");
//            esp.println(influxDbClient.getLastErrorMessage());
//        }
//        sensor.clearFields();

    // Read humidity
    float hum = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float cels = dht.readTemperature();
    // Compute heat index in Celsius (isFahrenheit = false)
    float hic = dht.computeHeatIndex(cels, hum, false);

    esp.print("Writing: cels = " + String(cels) + ", hum = " + String(hum) + ", hic = " + String(hic));
//        sensor.addField("cels", cels);
//        sensor.addField("hum", hum);
//        sensor.addField("hic", hic);
//        esp.println(sensor.toLineProtocol());

    // Check WiFi connection and reconnect if needed
    if ((WiFi.RSSI() == 0) && wifiMulti.run() != WL_CONNECTED) {
        esp.println("Wifi connection lost");
    }
    // Write point
//        if (!influxDbClient.writePoint(sensor)) {
//            esp.print("InfluxDB write failed: ");
//            esp.println(influxDbClient.getLastErrorMessage());
//        }

//        String influxClient = String(INFLUXDB_URL) + "/api/v2/write?org=" + INFLUXDB_ORG + "&bucket=" + INFLUXDB_BUCKET + "&precision=s";
//        Serial.printf("Influx [HTTPS] POST URL: ", influxClient);
//        String body = String("temperature _value=") + cels + " " +  "\n" + "humidity _value=" + hum + " " + "\n" +
//                      "index _value=" + hic + "\n";
//
//        httpInflux.addHeader("Authorization", INFLUXDB_TOKEN);
//        if (httpInflux.begin(*client, influxClient)) {  // HTTPS
//            esp.print("[HTTPS] POST...\n");
//            // start connection and send HTTP header
//           int httpCode = https.POST(body);
//            esp.print("[HTTPS] httpCode...\n" + (String) httpCode);
//            // httpCode will be negative on error
//            if (httpCode > 0) {
//                // HTTP header has been send and Server response header has been handled
//                esp.printf("[HTTPS] POST... code: %d\n", httpCode);
//
//               // file found at server
//                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
//                    String payload = https.getString();
//                    esp.println(payload);
//                }
//            } else {
//                esp.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
//            }
//
//            https.end();
//       } else {
//           esp.printf("[HTTPS] Unable to connect\n");
//       }

//       https.end();
    //   } else {
    //      esp.printf("[HTTPS] Unable to connect\n");
    //  }

    esp.println("Wait 10s before next round...");
    delay(10000);
}
