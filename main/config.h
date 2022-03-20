#define DEVICE "ESP8266"
#define TZ_INFO "MST-3MDT,M3.5.0/2,M10.5.0/3" // Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html, St. Petersburg
// DHT sensor
#define DHT_PIN 2
#define DHT_TYPE DHT22
// LED
#define LED_PIN 1
#define LED_CLK 2
#define LED_CS 3
// WIFI

#define WIFI_SSID "BD-2"
#define WIFI_PASSWORD "Sinabon181"
#define WL_CONNECTED 200
// InfluxDB
#define INFLUXDB_URL "https://europe-west1-1.gcp.cloud2.influxdata.com"
#define INFLUXDB_ORG "xelanotron@gmail.com"
#define INFLUXDB_BUCKET "arduino"
#define INFLUXDB_TOKEN "i46UR1ETkrURoN1mrZm9p1szsgwgik5ExajxBkhsNJUs8rv6imWMxzLgxcJbRPxd1nWna8DkHOKxVZRPhaILpA=="
// Loki
#define LOKI_USER "xelanotron"
#define LOKI_API_KEY "eyJrIjoiMTMyZGEwZDY3MzJiYjk0NDM3YWZjYzZhOTBmNGZjODBkZmY1Zjc2YyIsIm4iOiJhcmR1aW5vLWxvZ3MiLCJpZCI6NjA4NjYwfQ"
// Metrics
#define INTERVAL 30
// Graphite
#define GRAPHITE_USER "xelanotron"
#define GRAPHITE_API_KEY "api_key_graphite"
