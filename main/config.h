#define DEVICE "ESP8266"
// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Set timezone to St.Petersburg, Russia
#define TZ_INFO "MST-3MDT,M3.5.0/2,M10.5.0/3"
#define RX_PIN 0
#define TX_PIN 1
// DHT sensor
#define DHT_PIN 12
#define DHT_TYPE DHT22
// MH-Z19B sensor
#define MHZ_PIN 14 // pin for pwm reading
#define MHZ_TYPE MHZ19B
// DSM501A sensor
#define DUST_PIN 4
// WIFI
#define WIFI_SSID "wifiname"
#define WIFI_PASSWORD "password"

// InfluxDB
#define INFLUXDB_URL "url"
#define INFLUXDB_ORG "e-mail"
#define INFLUXDB_BUCKET "air"
#define INFLUXDB_TOKEN "token"
// InfluxDB environment
#define DHT_SENSOR "DHT22"
#define CO2_SENSOR "MHZ19B"
#define DUST_SENSOR "DSM501A"
// Loki
#define LOKI_USER "user"
#define LOKI_API_KEY "token"
// Metrics
#define INTERVAL 30
