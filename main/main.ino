#include <Arduino.h>
#include "config.h"
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <SoftwareSerial.h>
#include <MHZ.h>
#include <DHT.h>
#include <string.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

ESP8266WiFiMulti wifiMulti;
SoftwareSerial esp(RX_PIN, TX_PIN);
DHT dht(DHT_PIN, DHT_TYPE);
MHZ mhz(MHZ_PIN, MHZ_TYPE);

byte buff[2];
unsigned long duration;
unsigned long startTime;
unsigned long endTime;
unsigned long currentTime;
unsigned long sampleTimeMs = 3000;
unsigned long lowPulseOccupancy = 0;
float ratio = 0;
float concentration = 0;

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient influxDbClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// NTP Client
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP);
// Loki logs client
HTTPClient httpLoki;
// Data point
Point dhtPoint("air_state");
Point co2Point("air_state");
Point dustPoint("air_state");

void setup() {

    Serial.begin(115200);
    esp.begin(9600);

    // Wait 5s for serial connection or continue without it
    uint8_t serialTimeout;
    while (!Serial && serialTimeout < 50) {
        delay(100);
        serialTimeout++;
    }

//    for (uint8_t t = 4; t > 0; t--) {
//        Serial.printf("[SETUP] WAIT %d...\n", t);
//        Serial.flush();
//        delay(1000);
//    }

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
    if (mhz.isPreHeating()) {
        esp.print("MH-Z19B preheating");
        while (mhz.isPreHeating()) {
            esp.print(".");
            delay(10000);
        }
    }

    Serial.println();
    Serial.println();
    Serial.println();

    pinMode(DUST_PIN,INPUT);

    startTime = millis();
    currentTime = millis();

    // Add tags
    dhtPoint.addTag("sensor", DHT_SENSOR);
    co2Point.addTag("sensor", CO2_SENSOR);
    dustPoint.addTag("sensor", DUST_SENSOR);

    // Accurate time is necessary for certificate validation and writing in batches
    // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

    // Initialize a NTPClient to get time
    ntpClient.begin();

    // Check server connection
    if (influxDbClient.validateConnection()) {
        String logStr = "Connected to InfluxDB: " + String (influxDbClient.getServerUrl());
        esp.println(logStr);
        //esp.println(influxDbClient.getServerUrl());
    } else {
        String logStr = "InfluxDB connection failed: " + String (influxDbClient.getLastErrorMessage());
        esp.println(logStr);
        //esp.println(influxDbClient.getLastErrorMessage());
    }
}

void loop() {
    // Clear fields for reusing the point. Tags will remain untouched
    dhtPoint.clearFields();
    co2Point.clearFields();
    dustPoint.clearFields();

    float hum = dht.readHumidity();
    float temp = dht.readTemperature();
    float hic = dht.computeHeatIndex(temp, hum, false);
    esp.println("Writing: temp = " + String(temp) + ", hum = " + String(hum) + ", hic = " + String(hic));

    // Get current timestamp
    unsigned long ts = ntpClient.getEpochTime();
    esp.println("Current timestamp: " + String(ts));
    unsigned char* buf1 = new unsigned char[100];
    unsigned char* buf2 = new unsigned char[100];
    String lokiUrl = String("https://") + LOKI_USER + ":" + LOKI_API_KEY + "@" + LOKI_URI + "/loki/api/v1/push";
    String body = String("{\"streams\": [{ \"stream\": {\"monitoring_type\": \"air\"}, \"values\": [ [ \"") + ts + "000000000\", \"" + "temp=" + String(temp) + ", hum=" + String(hum) + ", hic=" + String(hic) + "\" ] ] }]}";

    esp.println("Submit POST request via HTTP...");
    // Submit POST request via HTTP
    std::unique_ptr<BearSSL::WiFiClientSecure>lokiClient(new BearSSL::WiFiClientSecure);
    lokiClient->setInsecure();
    httpLoki.begin(*lokiClient,lokiUrl);
    httpLoki.addHeader("Content-Type", "application/json");
    esp.println("Added header...");
    (String(LOKI_USER)).getBytes(buf1, 100, 0);
    (String(LOKI_API_KEY)).getBytes(buf2, 100, 0);
    httpLoki.setAuthorization((const char*)buf1, (const char*)buf2);
    esp.println("Set authorization...");
    int httpCode = httpLoki.POST(body);
    esp.println("Loki [HTTPS] POST...  Code: ");
    esp.print(httpCode);
    httpLoki.end();

    duration = pulseIn(DUST_PIN, LOW);
    lowPulseOccupancy += duration;
    endTime = millis();
    if ((endTime-currentTime) > sampleTimeMs){
        ratio = lowPulseOccupancy/(sampleTimeMs*10.0);  // Integer percentage 0=>100
        concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; // using spec sheet curve

        esp.println("lowpulseoccupancy: ");
        esp.print(lowPulseOccupancy);
        esp.print(" ratio: ");
        esp.print(ratio);
        esp.print(" concentration: ");
        esp.println(concentration);
        lowPulseOccupancy = 0;
        currentTime = millis();
    }

    int ppmPwm = 0;
    esp.println("Time: ");
    esp.println(String(millis() - startTime));
    if(120000 <= millis() - startTime) {
        ppmPwm = mhz.readCO2PWM();
        esp.println("PPMpwm: " + String(ppmPwm));
        esp.println("\n------------------------------");
        startTime = millis();
    }

    // Store measured value into point
    // Report RSSI of currently connected network
    dhtPoint.addField("temperature", temp);
    dhtPoint.addField("humidity", hum);
    dhtPoint.addField("hic", hic);
    if(ppmPwm != 0) {
        co2Point.addField("co2", ppmPwm);
    }
    dustPoint.addField("concentration", concentration);

    // Check WiFi connection and reconnect if needed
    if (wifiMulti.run() != WL_CONNECTED) {
        esp.println("Wifi connection lost");
    }

    // Write points
    if (!influxDbClient.writePoint(dhtPoint)) {
        esp.print("InfluxDB write dhtPoint failed: ");
        esp.println(influxDbClient.getLastErrorMessage());
    }
    if (!influxDbClient.writePoint(dustPoint)) {
        esp.print("InfluxDB write dustPoint failed: ");
        esp.println(influxDbClient.getLastErrorMessage());
    }

    // Construct a Flux queries
    // Query will find the worst RSSI for last hour for each connected WiFi network with this device
    // A query for DHT22
    String query;
    if(ppmPwm != 0) {
        if (!influxDbClient.writePoint(co2Point)) {
            esp.print("InfluxDB write co2Point failed: ");
            esp.println(influxDbClient.getLastErrorMessage());
        }
        // A query for MHZ19B
        query = "from(bucket: \"air\") \
        |> range(start: -1h) \
        |> filter(fn: (r) => r._measurement == \"air_state\" and r.tag == \"sensor\" and r._field == \"temperature\" and r._field == \"humidity\" and r._field == \"hic\") \
        |> filter(fn: (r) => r._measurement == \"air_state\" and r.tag == \"sensor\" and r._field == \"co2\") \
        |> filter(fn: (r) => r._measurement == \"air_state\" and r.tag == \"sensor\" and r._field == \"concentration\") \
        |> min()";
    } else {
        query  = "from(bucket: \"air\") \
        |> range(start: -1h) \
        |> filter(fn: (r) => r._measurement == \"air_state\" and r.tag == \"sensor\" and r._field == \"temperature\" and r._field == \"humidity\" and r._field == \"hic\") \
        |> filter(fn: (r) => r._measurement == \"air_state\" and r.tag == \"sensor\" and r._field == \"concentration\") \
        |> min()";
    }
    // Print composed query
    esp.println("Querying with: ");
    esp.println(query);

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
    }
    // Check if there was an error
    if(result.getError() != "") {
        esp.println("Query result error: ");
        esp.println(result.getError());
    }

    // Close the result
    result.close();

    esp.println("Wait 20s");
    delay(20000);
}
