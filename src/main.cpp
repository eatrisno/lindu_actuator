#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <Preferences.h>

// --- KONFIGURASI ---
#define RELAY_PIN 4 // Pin untuk Relay Sirine / Solenoid Pintu
const char* mqtt_server = "test.mosquitto.org"; // Ganti dengan mqtt.lindu.id nanti
const int mqtt_port = 1883;
const char* topic_alarm = "lindu/actuator/cmd/all";

// Lokasi Aktuator disimpan di NVS (Flash Memory)
Preferences preferences;
float my_lat = 0.0;
float my_lon = 0.0;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long alarm_start_time = 0;
bool is_alarming = false;
const unsigned long ALARM_DURATION_MS = 15000; // Sirine menyala 15 detik

// --- FUNGSI HAVERSINE (EDGE COMPUTING) ---
float haversine(float lat1, float lon1, float lat2, float lon2) {
    float dLat = (lat2 - lat1) * M_PI / 180.0;
    float dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = (lat1) * M_PI / 180.0;
    lat2 = (lat2) * M_PI / 180.0;

    float a = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1) * cos(lat2);
    float rad = 6371;
    float c = 2 * asin(sqrt(a));
    return rad * c;
}

// --- CALLBACK KETIKA ADA PESAN DARI SERVER ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (int i = 0; i < length; i++) msg += (char)payload[i];
    
    Serial.println("\n[<] Pesan diterima di " + String(topic));
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, msg);
    
    if (error) {
        Serial.println("Gagal parsing JSON");
        return;
    }

    String cmd = doc["cmd"] | "";
    if (cmd == "ALARM_ON") {
        float epi_lat = doc["epi_lat"];
        float epi_lon = doc["epi_lon"];
        float radius_km = doc["radius_km"];
        
        Serial.printf("[INFO] Pusat Gempa: %.4f, %.4f | Radius Bahaya: %.1f km\n", epi_lat, epi_lon, radius_km);
        
        // EDGE COMPUTING: Aktuator menghitung nasibnya sendiri
        if (my_lat == 0.0 && my_lon == 0.0) {
            Serial.println("[!] Lokasi belum diatur (Null Island). Abaikan gempa.");
            return;
        }
        
        float dist = haversine(my_lat, my_lon, epi_lat, epi_lon);
        Serial.printf("[MATH] Jarak Aktuator ini ke Pusat Gempa: %.2f km\n", dist);
        
        if (dist <= radius_km) {
            Serial.println("[!!!] KITA BERADA DI ZONA BAHAYA! MENYALAKAN SIRINE & MEMBUKA PINTU!");
            digitalWrite(RELAY_PIN, HIGH);
            is_alarming = true;
            alarm_start_time = millis();
        } else {
            Serial.println("[OK] Kita berada di luar zona bahaya gempa. Sirine tidak dinyalakan.");
        }
    } else if (cmd == "ALARM_OFF") {
        Serial.println("[INFO] Perintah ALARM_OFF Diterima. Mematikan Sirine!");
        digitalWrite(RELAY_PIN, LOW);
        is_alarming = false;
    } else if (cmd == "RESET") {
        Serial.println("[!!!] Perintah RESET Diterima. Me-restart Aktuator...");
        ESP.restart();
    }
}

unsigned long last_reconnect_time = 0;

void reconnectMQTT() {
    if (!client.connected()) {
        if (millis() - last_reconnect_time < 5000) return; // Non-blocking
        last_reconnect_time = millis();
        
        Serial.print("Menyambungkan ke MQTT... ");
        String clientId = "LinduActuator-" + String(ESP.getEfuseMac(), HEX);
        
        if (client.connect(clientId.c_str())) {
            Serial.println("Terhubung!");
            client.subscribe(topic_alarm);
        } else {
            Serial.print("Gagal, rc=");
            Serial.println(client.state());
        }
    }
}

#ifndef PIO_UNIT_TESTING

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    
    Serial.println("\n--- Lindu.id Actuator Node ---");
    
    // Baca memori internal (NVS) untuk koordinat terakhir
    preferences.begin("lindu", false);
    my_lat = preferences.getFloat("lat", 0.0);
    my_lon = preferences.getFloat("lon", 0.0);

    WiFiManager wm;
    
    // Siapkan Custom HTML Form untuk Latitude & Longitude
    char lat_str[20]; char lon_str[20];
    sprintf(lat_str, "%.6f", my_lat);
    sprintf(lon_str, "%.6f", my_lon);
    
    WiFiManagerParameter custom_lat("lat", "Latitude (Contoh: -6.20)", lat_str, 20);
    WiFiManagerParameter custom_lon("lon", "Longitude (Contoh: 106.81)", lon_str, 20);
    
    wm.addParameter(&custom_lat);
    wm.addParameter(&custom_lon);

    // wm.resetSettings(); // Buka komentar ini jika ingin mengetes ulang halaman Captive Portal
    
    if (!wm.autoConnect("Lindu-Actuator-Setup")) {
        Serial.println("Gagal konek WiFi dan timeout");
        ESP.restart();
    }
    
    Serial.println("WiFi Terhubung!");
    
    // Ambil hasil inputan dari halaman Web, update ke memori ESP32
    my_lat = atof(custom_lat.getValue());
    my_lon = atof(custom_lon.getValue());
    preferences.putFloat("lat", my_lat);
    preferences.putFloat("lon", my_lon);
    preferences.end();
    
    Serial.printf("[GPS] Lokasi Aktuator tersimpan: %.6f, %.6f\n", my_lat, my_lon);
    
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqttCallback);
}

void loop() {
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();
    
    if (is_alarming && (millis() - alarm_start_time >= ALARM_DURATION_MS)) {
        Serial.println("[OK] Mematikan Sirine (Timeout 15s)");
        digitalWrite(RELAY_PIN, LOW);
        is_alarming = false;
    }
}

#endif // PIO_UNIT_TESTING
