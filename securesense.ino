#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Update.h>
#include "DHT.h"
#include <AESLib.h>
#include <mbedtls/md.h> // For HMAC-SHA256

#define DHTPIN 15
#define DHTTYPE DHT22

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT settings
const char* mqtt_server = "your-mqtt-server.com";
const int mqtt_port = 8883;
# const char* mqtt_user = "device_user";
# const char* mqtt_password = "device_password";
# const char* mqtt_topic = "sensors/device1";

// AES-256 + HMAC keys (replace by own key)
const char aes_key[] = "this_is_a_32_byte_key_for_aes_256!!"; // 32 B
const char hmac_key[] = "this_is_another_32_byte_secret!!"; // 32 B

DHT dht(DHTPIN, DHTTYPE);
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

char jsonBuffer[128];
char encryptedBuffer[256];
uint8_t hmacResult[32]; // SHA-256 to get 32-byte hash

// Timing
unsigned long previousMillis = 0;
const long interval = 5000; // 5 sec

// Root CA certificate (for MQTT TLS)
// Replace with your server's CA cert
// config_secure.h (Add to .gitignore)
const char* root_ca = "-----BEGIN CERTIFICATE-----\n" \
                      "YOUR_ACTUAL_CERT_HERE\n" \
                      "-----END CERTIFICATE-----\n";

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  
  // Initialize DHT sensor
  if (!dht.begin()) {
    Serial.println("DHT init failed!");
    while (true);
  }

  // Connect to WiFi
  connectWiFi();

  // Configuring MQTT TLS
  wifiClient.setCACert(root_ca);
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback); // For incoming messages
}

// ===================== HMAC-SHA256 =====================
void generateHMAC(const char* data, size_t len) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)hmac_key, strlen(hmac_key));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)data, len);
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
}

// ===================== ENCRYPTION =====================
String encryptData(const char* plaintext) {
  byte iv[16];
  // Generate random IV
  for (int i = 0; i < sizeof(iv); i++) iv[i] = random(256);

  // Encrypt (AES-256-CBC)
  AESLib aes;
  aes.set_IV(iv);
  int encryptedLen = aes.encrypt(plaintext, encryptedBuffer, aes_key, 256, iv);
  encryptedBuffer[encryptedLen] = '\0';

  // Generate HMAC
  generateHMAC(encryptedBuffer, encryptedLen);

  // Combine IV + Ciphertext + HMAC
  String output;
  output.reserve(16 + encryptedLen + 32); // IV(16) + ciphertext + HMAC(32)
  
  // Add IV (hex)
  for (int i = 0; i < 16; i++) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", iv[i]);
    output += hex;
  }

  // Add ciphertext (hex)
  for (int i = 0; i < encryptedLen; i++) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", (uint8_t)encryptedBuffer[i]);
    output += hex;
  }

  // Add HMAC (hex)
  for (int i = 0; i < 32; i++) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", hmacResult[i]);
    output += hex;
  }

  return output;
}

// ===================== MQTT =====================
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Connected!");
      mqttClient.subscribe(mqtt_topic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 5s...");
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming messages (if needed)
}

// ===================== MAIN LOOP =====================
void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis < interval) return;
  previousMillis = currentMillis;

  // Read sensor
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Sensor read failed!");
    return;
  }

  // Create JSON
  snprintf(jsonBuffer, sizeof(jsonBuffer),"{\"temp\":%.1f,\"hum\":%.1f}", temp, hum);

  // Encrypt + HMAC
  String encrypted = encryptData(jsonBuffer);

  // Publish to MQTT
  if (mqttClient.publish(mqtt_topic, encrypted.c_str())) {
    Serial.println("Data published securely!");
  } else {
    Serial.println("Publish failed!");
  }
}

// ===================== WIFI =====================
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi failed! Retrying...");
    delay(5000);
    connectWiFi(); // Recursive retry
  } else {
    Serial.println("\nWiFi connected!");
  }
}
