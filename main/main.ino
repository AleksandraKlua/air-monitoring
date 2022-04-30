#include <Arduino.h>
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <SoftwareSerial.h>
#include <MHZ.h>
#include <DHT.h>
#include <WiFiClient.h>
#include <string.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

ESP8266WiFiMulti wifiMulti;
SoftwareSerial esp(RX_PIN, TX_PIN);
DHT dht(DHT_PIN, DHT_TYPE);
MHZ mhz(MHZ_PIN, MHZ_TYPE);

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient influxDbClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// Data point
Point point("air_state");
Point point2("air_state");

byte buff[2];
unsigned long duration;
unsigned long startTime;
unsigned long endTime;
unsigned long currentTime;
unsigned long sampleTimeMs = 3000;
unsigned long lowPulseOccupancy = 0;
float ratio = 0;
float concentration = 0;

void setup() {

    Serial.begin(115200);
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
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

    Serial.println();
    Serial.println();
    Serial.println();

    dht.begin();

    Serial.println();
    Serial.println();
    Serial.println();

    pinMode(MHZ_PIN, INPUT);

    // MH-Z19B must heating before work for almost two minutes
    esp.println("MHZ 19B");
    if (mhz.isPreHeating()) {
        esp.print("Preheating");
        while (mhz.isPreHeating()) {
            esp.print(".");
            delay(10000);
        }
    }

    Serial.println();
    Serial.println();
    Serial.println();

    startTime = millis()/1000;

    // Add tags
    point.addTag("dhtSensor", DHT_SENSOR);
    point.addTag("co2Sensor", CO2_SENSOR);
    //   point.addTag("dustSensor", DUST_SENSOR);

    // Accurate time is necessary for certificate validation and writing in batches
    // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

    // Check server connection
    if (influxDbClient.validateConnection()) {
        esp.println("Connected to InfluxDB: ");
        esp.println(influxDbClient.getServerUrl());
    } else {
        esp.println("InfluxDB connection failed: ");
        esp.println(influxDbClient.getLastErrorMessage());
    }
}

void loop() {
    // Clear fields for reusing the point. Tags will remain untouched
    point.clearFields();
    //point2.clearFields();

    float hum = dht.readHumidity();
    float temp = dht.readTemperature();
    float hic = dht.computeHeatIndex(temp, hum, false);
    esp.println("Writing: temp = " + String(temp) + ", hum = " + String(hum) + ", hic = " + String(hic));

    esp.println("\n----- Time from start: ");
    esp.print(startTime);
    esp.print(" s");

    currentTime = millis() / 1000;
    esp.println("\n----- Current time: ");
    esp.print(currentTime);
    esp.print(" s");

    int ppmPwm = 0;
    if ((currentTime - startTime) >= 120) {
        ppmPwm = mhz.readCO2PWM();
        esp.println("PPMpwm: ");
        esp.print(ppmPwm);
        esp.println("\n------------------------------");
        startTime =  millis() / 1000;
    }

    //delay(5000);

    // Store measured value into point
    // Report RSSI of currently connected network
    point.addField("temperature", temp);
    point.addField("humidity", hum);
    point.addField("hic", hic);
    if(ppmPwm != 0) {
        point.addField("co2", ppmPwm);
    }

    // Print what are we exactly writing
    esp.println("Writing: ");
    esp.println(point.toLineProtocol());
//    esp.println("Writing2: ");
//    esp.println(point2.toLineProtocol());

    // Check WiFi connection and reconnect if needed
    if (wifiMulti.run() != WL_CONNECTED) {
        esp.println("Wifi connection lost");
    }

    // Write point
    if (!influxDbClient.writePoint(point)) {
        esp.print("InfluxDB write failed: ");
        esp.println(influxDbClient.getLastErrorMessage());
    }

    esp.println("Wait 10s");
    delay(10000);
    // Construct a Flux queries
    // Query will find the worst RSSI for last hour for each connected WiFi network with this device
    // A query for DHT22
    String query;
    if(ppmPwm != 0) {
        // A query for MHZ19B
        query = "from(bucket: \"air\") \
        |> range(start: -1h) \
        |> filter(fn: (r) => r._measurement == \"air_state\" and r.tag == \"dhtSensor\") \
        |> filter(fn: (r) => r._field == \"temperature\" and r._field == \"humidity\" and r._field == \"hic\") \
        |> filter(fn: (r) => r._measurement == \"air_state\" and r.tag == \"co2Sensor\") \
        |> filter(fn: (r) => r._field == \"co2\") \
        |> min()";
        // Print composed query
        esp.println("Querying with: ");
        esp.println(query);
//        // Print output header
//        esp.println("==========");
//        // Send query to the server and get result
//        FluxQueryResult result = influxDbClient.query(query);
//
//        // Iterate over rows. Even there is just one row, next() must be called at least once.
//        while (result.next()) {
//            // Get converted value for the _time column
//            FluxDateTime time = result.getValueByName("_time").getDateTime();
//
//            // Format date-time for printing
//            // Format string according to http://www.cplusplus.com/reference/ctime/strftime/
//            String timeStr = time.format("%F %T");
//
//            esp.print(" at ");
//            esp.print(timeStr);
//
//            esp.println();
//        }
//        // Check if there was an error
//        if(result.getError() != "") {
//            Serial.print("Query result error: ");
//            Serial.println(result.getError());
//        }
//
//        // Close the result
//        result.close();
    } else {
        query  = "from(bucket: \"air\") \
        |> range(start: -1h) \
        |> filter(fn: (r) => r._measurement == \"air_state\" and r.tag == \"dhtSensor\") \
        |> filter(fn: (r) => r._field == \"temperature\" and r._field == \"humidity\" and r._field == \"hic\") \
        |> min()";
        // Print composed query
        esp.println("Querying with DHT22 only: ");
        esp.println(query);
    }


    // Print output header
    esp.println("==========");
    // Send query to the server and get result
    FluxQueryResult result = influxDbClient.query(query);

    // Iterate over rows. Even there is just one row, next() must be called at least once.
    while (result.next()) {
        // Get converted value for the _time column
        FluxDateTime time = result.getValueByName("_time").getDateTime();

        // Format date-time for printing
        // Format string according to http://www.cplusplus.com/reference/ctime/strftime/
        String timeStr = time.format("%F %T");

        esp.print(" at ");
        esp.print(timeStr);

        esp.println();
    }
    // Check if there was an error
    if(result.getError() != "") {
        Serial.print("Query result error: ");
        Serial.println(result.getError());
    }

    // Close the result
    result.close();

    esp.println("Wait 10s");
    delay(10000);
}
