#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>
#include <MHZ.h>
#include <WiFiClient.h>
#include <SoftwareSerial.h>
#include "config.h"

#define SENSOR "DHT22"
#define CO2_SENSOR "MHZ19B"
// pin for pwm reading
#define CO2_IN 14
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Set timezone to St.Petersburg, Russia
#define TZ_INFO "MST-3MDT,M3.5.0/2,M10.5.0/3"

ESP8266WiFiMulti wifiMulti;

int rxPin = 0;
int txPin = 1;
SoftwareSerial esp(rxPin, txPin);

DHT dht(DHT_PIN, DHT_TYPE);
MHZ co2(CO2_IN, MHZ19B);

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient influxDbClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Data point
Point sensor("air_state");

unsigned long currentTime;


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

    pinMode(CO2_IN, INPUT);
    esp.println("MHZ 19B");
    if (co2.isPreHeating()) {
        esp.print("Preheating");
        while (co2.isPreHeating()) {
            esp.print(".");
            delay(5000);
        }
        esp.println();
    }

    Serial.println();
    Serial.println();
    Serial.println();

    //currentTime = millis()/1000;

    // Add tags
    sensor.addTag("sensor", SENSOR);
    //sensor.addTag("co2sensor", CO2_SENSOR);

    // Accurate time is necessary for certificate validation and writing in batches
    // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

    // Check server connection
    if (influxDbClient.validateConnection()) {
        esp.print("Connected to InfluxDB: ");
        esp.println(influxDbClient.getServerUrl());
    } else {
        esp.print("InfluxDB connection failed: ");
        esp.println(influxDbClient.getLastErrorMessage());
    }
}

void loop() {
    // Clear fields for reusing the point. Tags will remain untouched
    sensor.clearFields();

    float hum = dht.readHumidity();
    float temp = dht.readTemperature();
    float hic = dht.computeHeatIndex(temp, hum, false);
    esp.println("Writing: temp = " + String(temp) + ", hum = " + String(hum) + ", hic = " + String(hic));

    esp.print("\n----- Time from start: ");
    esp.print(millis() / 1000);
    esp.println(" s");

    int ppm_pwm = co2.readCO2PWM();
    esp.print("PPMpwm: ");
    esp.print(ppm_pwm);
    esp.println("\n------------------------------");
    //delay(5000);

    // Store measured value into point
    // Report RSSI of currently connected network
    sensor.addField("temperature", temp);
    sensor.addField("humidity", hum);
    sensor.addField("hic", hic);
    sensor.addField("co2", ppm_pwm);

    // Print what are we exactly writing
    Serial.print("Writing: ");
    Serial.println(sensor.toLineProtocol());

    // Check WiFi connection and reconnect if needed
    if (wifiMulti.run() != WL_CONNECTED) {
        esp.println("Wifi connection lost");
    }

    // Write point
    if (!influxDbClient.writePoint(sensor)) {
        esp.print("InfluxDB write failed: ");
        esp.println(influxDbClient.getLastErrorMessage());
    }

    esp.println("Wait 10s");
    delay(10000);

    // Construct a Flux query
    // Query will find the worst RSSI for last hour for each connected WiFi network with this device
    String query = "from(bucket: \"air\") \
    |> range(start: -1h) \
    |> filter(fn: (r) => r._measurement == \"air_state\" and r._field == \"temperature\" and r._field == \"humidity\" and r._field == \"hic\" and r._field == \"co2\") \
    |> min()";
    // Print composed query
    esp.print("Querying with: ");
    esp.println(query);

    // Print ouput header
    esp.print("==========");
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