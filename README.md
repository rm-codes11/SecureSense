# SecureSense
ESP32 encrypts sensor data (AES-256 + HMAC) → Flask server decrypts with rate limiting + OTA updates

## Overview
A secure system where an **ESP32 microcontroller** encrypts sensor data (AES-256-CBC + HMAC-SHA256) and transmits it to a **Flask server** that decrypts and logs the data. Supports:
1. End-to-end encryption
2. MQTT over TLS
3. Secure OTA firmware updates
4. Rate-limited API with brute force protection

## Hardware Requirements
1. ESP32 board
2. DHT22 (humidity and temperature sensor)
3. Breadboard + jumper cables

## Server requirements
1. Python 3.8+
2. Windows/macOS/Linux host machine
3. OpenSSL (for certificate generation)

## Installation
1. Install Arduino IDE with ESP32 support
2. Clone this repo
3. Install required libraries
   ```
   arduino-cli lib install "DHT sensor library" "AESLib" "PubSubClient"
   ```
4. Update WiFi credentials in securesense.ino file

## Security configuration
1. Generate encryption keys (in bash):
```
openssl rand -hex 32 > aes_key.txt
openssl rand -hex 32 > hmac_key.txt
```
2. Configure environment variables (in bash):
```
export AES_KEY=$(cat aes_key.txt)
export HMAC_KEY=$(cat hmac_key.txt)
```
## Deployment
1. Compile and flash the securesense.ino file onto ESP32 via Arduino IDE
2. Monitor serial output at 115200 baud
3. At local server 
```
gunicorn --bind 0.0.0.0:443 --certfile cert.pem --keyfile key.pem app:app
```

## API Endpoints
1. `/upload` → POST method: to receive encrypted sensor data
2. `/firmware` → GET method: to provide OTA updates

## Data Flow
```mermaid
flowchart TD
    subgraph ESP32
        A[DHT22 Sensor] -->|Temp/Humidity| B[Encrypt Data\nAES-256 + HMAC]
        B --> C[MQTT over TLS]
    end

    subgraph Cloud
        C --> D[Flask Server]
        D -->|Decrypt| E[Verify HMAC]
        E --> F[Store in Database]
    end

    subgraph Mobile
        F --> G[Admin Dashboard]
    end

    style ESP32 fill:##80ffd4,stroke:#333
    style Cloud fill:#ffb3cc,stroke:#333
