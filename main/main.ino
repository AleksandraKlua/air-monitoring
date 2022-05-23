#include <Arduino.h>
#include "config.h"
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <SoftwareSerial.h>
#include <MHZ.h>
#include <DHT.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

ESP8266WiFiMulti wifiMulti;
SoftwareSerial esp(RX_PIN, TX_PIN);
DHT dht(DHT_PIN, DHT_TYPE);
MHZ mhz(MHZ_PIN, MHZ_TYPE);

unsigned long duration;
unsigned long startTime;
unsigned long endTime;
unsigned long currentTime;
unsigned long sampleTimeMs = 30000;
unsigned long lowPulseOccupancy = 0;
float ratio = 0;
float concentration = 0;

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient influxDbClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// Data point
Point dhtPoint("air_state");
Point co2Point("air_state");
Point dustPoint("air_state");

void setup() {
    Serial.begin(115200);
    esp.begin(9600);

    // Wait 5s for serial connection or continue without it
    uint8_t serialTimeout;
    while (!Serial && serialTimeout < 5) {
        delay(1000);
        serialTimeout++;
    }

    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

    dht.begin();

    pinMode(MHZ_PIN, INPUT);
    // MH-Z19B must heating before work for almost two minutes
    if (mhz.isPreHeating()) {
        esp.print("MH-Z19B preheating");
        while (mhz.isPreHeating()) {
            esp.print(".");
            digitalWrite(LED_BUILTIN, LOW);
            delay(1000);
            digitalWrite(LED_BUILTIN, HIGH);
            delay(9000);
        }
    }

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

    // Check server connection
    if (influxDbClient.validateConnection()) {
        esp.println("Connected to InfluxDB: " + String (influxDbClient.getServerUrl()));
    } else {
        esp.println("InfluxDB connection failed: " + String (influxDbClient.getLastErrorMessage()));
    }
}

void loop() {
    // Clear fields for reusing the point. Tags will remain untouched
    dhtPoint.clearFields();
    co2Point.clearFields();
    dustPoint.clearFields();

    storeDhtValuesIntoPoint();
    storeDustConcentrationIntoPoint();

    // Check WiFi connection and reconnect if needed
    if (wifiMulti.run() != WL_CONNECTED) {
        esp.println("Wifi connection lost");
    }

    esp.println("Time: " + String(millis() - startTime));
    if(120000 <= millis() - startTime) {
        storeCo2IntoPoint();
        // Write point
        checkWritingPointToInflux(co2Point, "co2Point");
        // A query with MHZ19B
        String co2Query = createFluxQuery("\"co2\"");
        // Print query
        esp.println("CO2 querying with:\n" + co2Query);

        sendQuery(co2Query);
        delay(1000); //send request in one second to avoid esp8266 overloading
    }

    // Write points
    checkWritingPointToInflux(dhtPoint, "dhtPoint");
    checkWritingPointToInflux(dustPoint, "dustPoint");
    // Construct a Flux queries
    // Query will find the worst RSSI for last hour for each connected WiFi network with this device
    String dhtQuery = createFluxQuery("\"temperature\" and r._field == \"humidity\" and r._field == \"hic\"");
    String dustQuery = createFluxQuery("\"concentration\"");
    // Print composed query
    esp.println("DHT querying with:\n" + dhtQuery);
    esp.println("DUST querying with:\n" + dustQuery);

    sendQuery(dhtQuery);
    delay(1000); //send request in one second
    sendQuery(dustQuery);

    esp.println("Wait 20s");
    delay(20000);
}

void storeDhtValuesIntoPoint() {
    float hum = dht.readHumidity();
    float temp = dht.readTemperature();
    float hic = dht.computeHeatIndex(temp, hum, false);
    esp.println("Writing: temp = " + String(temp) + ", hum = " + String(hum) + ", hic = " + String(hic));
    // Store measured value into point
    dhtPoint.addField("temperature", temp);
    dhtPoint.addField("humidity", hum);
    dhtPoint.addField("hic", hic);
}

void storeDustConcentrationIntoPoint() {
    duration = pulseIn(DUST_PIN, LOW); // in µs
    lowPulseOccupancy += duration;
    endTime = millis();
    if ((endTime-currentTime) > sampleTimeMs){
        ratio = lowPulseOccupancy/(sampleTimeMs*10.0);  // 1s * 1e-6 / (3*10)s * 100% = 1s / (3 * 1e5)s
        concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; // using spec sheet curve mg/m3

        esp.print("lowPulseOccupancy: ");
        esp.println(lowPulseOccupancy);
        esp.print("ratio: ");
        esp.println(ratio);
        esp.print("concentration: ");
        esp.println(concentration);

        lowPulseOccupancy = 0;
        currentTime = millis();
    }
    dustPoint.addField("concentration", concentration);
}

void storeCo2IntoPoint() {
    int ppmPwm = mhz.readCO2PWM();
    esp.println("PPMpwm: " + String(ppmPwm));
    startTime = millis();
    co2Point.addField("co2", ppmPwm);
}

String createFluxQuery(String fieldsName) {
    return "from(bucket: \"air\") \
        |> range(start: -1h) \
        |> filter(fn: (r) => r._measurement == \"air_state\" and r.tag == \"sensor\" and r._field == " + fieldsName + ")";
}

void checkWritingPointToInflux(Point point, String pointName) {
    if (!influxDbClient.writePoint(point)) {
        esp.println("InfluxDB write "+ pointName +" failed: " + influxDbClient.getLastErrorMessage());
    }
}

void sendQuery(String query) {
    FluxQueryResult result = influxDbClient.query(query);
    // Iterate over rows. Even there is just one row, next() must be called at least once.
    while (result.next()) {
        // Get converted value for the _time column
        FluxDateTime time = result.getValueByName("_time").getDateTime();
        // Format date-time for printing
        // Format string according to http://www.cplusplus.com/reference/ctime/strftime/
        String timeStr = time.format("%F %T");
        esp.print(" at " + timeStr);
    }
    // Check if there was an error
    if(result.getError() != "") {
        esp.println("Query result error:\n" + result.getError());
    }
    // Close the result
    result.close();
}
