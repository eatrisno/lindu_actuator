#include <Arduino.h>
#include <unity.h>
#include <ArduinoJson.h>
#include <math.h>

// Fungsi kembar dari main.cpp untuk diuji
float test_haversine(float lat1, float lon1, float lat2, float lon2) {
    float dLat = (lat2 - lat1) * M_PI / 180.0;
    float dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = (lat1) * M_PI / 180.0;
    lat2 = (lat2) * M_PI / 180.0;

    float a = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1) * cos(lat2);
    float rad = 6371;
    float c = 2 * asin(sqrt(a));
    return rad * c;
}

void setUp(void) {}
void tearDown(void) {}

// ==============================================
// TEST 1: Actuator Berada di ZONA BAHAYA
// ==============================================
void test_actuator_in_danger_zone(void) {
    // Episentrum Gempa (misal di Bandung)
    float epi_lat = -6.9147; 
    float epi_lon = 107.6098;
    float radius_km = 100.0; // Peringatan radius 100km
    
    // Aktuator terpasang di Cimahi (Dekat Bandung)
    float my_lat = -6.8723;
    float my_lon = 107.5436;
    
    float dist = test_haversine(my_lat, my_lon, epi_lat, epi_lon);
    
    // Jarak Cimahi-Bandung sekitar 8km. Harus menyalakan sirine (< 100km)
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(radius_km, dist);
}

// ==============================================
// TEST 2: Actuator Berada di ZONA AMAN
// ==============================================
void test_actuator_safe_zone(void) {
    // Episentrum di Padang
    float epi_lat = -0.9471; 
    float epi_lon = 100.4172;
    float radius_km = 150.0; // Peringatan Radius 150km
    
    // Aktuator terpasang di Jakarta
    float my_lat = -6.2088;
    float my_lon = 106.8456;
    
    float dist = test_haversine(my_lat, my_lon, epi_lat, epi_lon);
    
    // Jarak sekitar 900km, Aktuator harus AMAN dan sirine mati (> 150km)
    TEST_ASSERT_GREATER_THAN_FLOAT(radius_km, dist);
}

// ==============================================
// TEST 3: Parsing Payload Alarm dari MQTT (JSON)
// ==============================================
void test_json_parsing_command(void) {
    // Simulasi pesan yang dikirim oleh Server
    String payload = "{\"cmd\":\"ALARM_ON\",\"level\":\"CRITICAL\",\"epi_lat\":-6.56,\"epi_lon\":107.22,\"radius_km\":100.0}";
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    // Memastikan library ArduinoJson (v7) bisa memecah string dengan sukses
    TEST_ASSERT_FALSE(error);
    
    String cmd = doc["cmd"];
    float epi_lat = doc["epi_lat"];
    
    TEST_ASSERT_EQUAL_STRING("ALARM_ON", cmd.c_str());
    TEST_ASSERT_EQUAL_FLOAT(-6.56, epi_lat);
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    
    RUN_TEST(test_actuator_in_danger_zone);
    RUN_TEST(test_actuator_safe_zone);
    RUN_TEST(test_json_parsing_command);
    
    UNITY_END();
}

void loop() {}
