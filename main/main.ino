#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WiFiMulti.h>
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>
#include <LedControl.h>
#include "config.h" // Configuration file
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// NTP Client
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP);

// WiFi
ESP8266WiFiMulti wifiMulti;

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Data point
Point sensor("wifi_status");

// Loki & Influx Clients
HTTPClient httpInflux;
HTTPClient httpLoki;
HTTPClient httpGraphite;

// Sensors
DHT dht(DHT_PIN, DHT_TYPE);

// LED
LedControl lc = LedControl(LED_PIN, LED_CLK, LED_CS,
                           0); // (first 3 arguments are the pin-numbers: dataPin,clockPin,csPin,numDevices)

// LED visualisations
byte smile[8] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};
byte neutral[8] = {0x3C, 0x42, 0xA5, 0x81, 0xBD, 0x81, 0x42, 0x3C};
byte sad[8] = {0x3C, 0x42, 0xA5, 0x81, 0x99, 0xA5, 0x42, 0x3C};
byte off[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
byte error[8] = {0x00, 0x00, 0x78, 0x40, 0x70, 0x40, 0x78, 0x00};

// Function to set up the connection to the WiFi AP
void setupWiFi() {
    Serial.print("Connecting to '");
    Serial.print(WIFI_SSID);
    Serial.print("' ...");

//  WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

    while (wifiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(100);
    }

    Serial.println("connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Add tags
    sensor.addTag("device", DEVICE);
    sensor.addTag("SSID", WiFi.SSID());
}

void connectToInflux() {
    // Add tags
    sensor.addTag("device", DEVICE);
    sensor.addTag("SSID", WiFi.SSID());

    // Accurate time is necessary for certificate validation and writing in batches
    // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

    // Check server connection
    if (client.validateConnection()) {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(client.getServerUrl());
    } else {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(client.getLastErrorMessage());
    }
}

// Function to submit metrics to Influx
void submitToInflux(unsigned long ts, float cels, float hum, float hic) {
    String influxClient =
            String(INFLUXDB_URL) + "/api/v2/write?org=" + INFLUXDB_ORG + "&bucket=" + INFLUXDB_BUCKET + "&precision=s";
    Serial.printf("Influx [HTTPS] POST URL: ", influxClient);
    String body = String("temperature value=") + cels + " " + ts + "\n" + "humidity value=" + hum + " " + ts + "\n" +
                  "index value=" + hic + " " + ts + "\n";

    // Submit POST request via HTTP
    httpInflux.begin(influxClient);
    httpInflux.addHeader("Authorization", INFLUXDB_TOKEN);
    int httpCode = httpInflux.POST(body);
    Serial.printf("Influx [HTTPS] POST...  Code: %d\n", httpCode);
    httpInflux.end();
}

// Function to submit logs to Loki
void submitToLoki(unsigned long ts, float cels, float hum, float hic, String message) {
    String lokiClient =
            String("https://") + LOKI_USER + ":" + LOKI_API_KEY + "@logs-prod-us-central1.grafana.net/loki/api/v1/push";
    String body =
            String("{\"streams\": [{ \"stream\": { \"date\": \"2022_03_06\", \"monitoring_type\": \"air\"}, \"values\": [ [ \"") +
            ts + "000000000\", \"" + "temperature=" + cels + " humidity=" + hum + " heat_index=" + hic + " status=" +
            message + "\" ] ] }]}";

    // Submit POST request via HTTP
    httpLoki.begin(lokiClient);
    httpLoki.addHeader("Content-Type", "application/json");
    int httpCode = httpLoki.POST(body);
    Serial.printf("Loki [HTTPS] POST...  Code: %\n", httpCode);
    httpLoki.end();
}

// Function to submit logs to Graphite
void submitToGraphite(unsigned long ts, float cels, float hum, float hic) {
    // build hosted metrics json payload
    String body = String("[") +
                  "{\"name\":\"temperature\",\"interval\":" + INTERVAL + ",\"value\":" + cels +
                  ",\"mtype\":\"gauge\",\"time\":" + ts + "}," +
                  "{\"name\":\"humidity\",\"interval\":" + INTERVAL + ",\"value\":" + hum +
                  ",\"mtype\":\"gauge\",\"time\":" + ts + "}," +
                  "{\"name\":\"heat_index\",\"interval\":" + INTERVAL + ",\"value\":" + hic +
                  ",\"mtype\":\"gauge\",\"time\":" + ts + "}]";

    // submit POST request via HTTP
    httpGraphite.begin("https://graphite-prod-01-eu-west-0.grafana.net/graphite/metrics");
    httpGraphite.setAuthorization(GRAPHITE_USER, GRAPHITE_API_KEY);
    httpGraphite.addHeader("Content-Type", "application/json");

    int httpCode = httpGraphite.POST(body);
    Serial.printf("Graphite [HTTPS] POST...  Code: %\n", httpCode);
    httpGraphite.end();
}

// Function to display state of the plant on LED Matrix
void displayState(byte character[]) {
    unsigned long currHour = ntpClient.getHours();
    bool shouldDisplay = (currHour < 23 && currHour > 8);   // Turn off in the night
    if (shouldDisplay) {
        for (int i = 0; i < 8; i++) {
            lc.setRow(0, i, character[i]);
        }
    } else {
        for (int i = 0; i < 8; i++) {
            lc.setRow(0, i, off[i]);
        }
    }
}

// Function to check if any reading failed
bool checkIfReadingFailed(float hum, float cels) {
    if (isnan(hum) || isnan(cels)) {
        // Print letter E as error
        displayState(error);
        Serial.println(F("Failed to read from some sensor!"));
        return true;
    }
    return false;
}

// Function to create message and display current state of the plant
String createAndDisplayState(int hic) {
    String currentState = "";
    if (hic >= 54) {
        currentState = "Serious risk to health";
        displayState(sad);
    } else if (hic >= 32) {
        currentState = "Prolonged exposure and activity could lead to heatstroke";
        displayState(neutral);
    } else if (hic >= 27) {
        currentState = "Prolonged exposure and activity may lead to heatstroke";
        displayState(neutral);
    } else {
        currentState = "OK info";
        displayState(smile);
    }

    return currentState;
}

// ========== MAIN FUNCTIONS: SETUP & LOOP ==========
// SETUP: Function called at boot to initialize the system
void setup() {
    // Start the serial output at 115,200 baud
    Serial.begin(115200);

    // Connect to WiFi
    setupWiFi();

    // Connect to InfluxDB
    connectToInflux();

    // Initialize a NTPClient to get time
    ntpClient.begin();

    // Start the DHT sensor
    dht.begin();

    // Start LED Matrix
    lc.shutdown(0, false);
    lc.setIntensity(0, 0.0000001);      //Adjust the brightness maximum is 15
    lc.clearDisplay(0);
}

// LOOP: Function called in a loop to read from sensors and send them do databases
void loop() {
    // Reconnect to WiFi if required
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        yield();
        setupWiFi();
        connectToInflux();
    }

    // Update time via NTP if required
    while (!ntpClient.update()) {
        yield();
        ntpClient.forceUpdate();
    }

    // Get current timestamp
    unsigned long ts = ntpClient.getEpochTime();

    // Read humidity
    float hum = dht.readHumidity();

    // Read temperature as Celsius (the default)
    float cels = dht.readTemperature();

    // Check if any reads failed and return early
    if (checkIfReadingFailed(hum, cels)) {
        return;
    }

    // Compute heat index in Celsius (isFahrenheit = false)
    float hic = dht.computeHeatIndex(cels, hum, false);

    // Convert data to state of the air
    String message = createAndDisplayState(hic);

    // Submit data
    submitToInflux(ts, cels, hum, hic);
    submitToGraphite(ts, cels, hum, hic);
    submitToLoki(ts, cels, hum, hic, message);

    // wait INTERVAL seconds, then do it again
    Serial.println("Wait 10s");
    delay(INTERVAL * 1000);
}