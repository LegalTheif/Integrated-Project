#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// LED pin definitions
const int GREEN_LED = 5;   // D1 - GPIO5
const int YELLOW_LED = 4;  // D2 - GPIO4
const int RED_LED = 0;     // D3 - GPIO0

// Traffic state variables
int vehicleCount = 0;
bool ambulanceDetected = false;

// Async web server on port 80
AsyncWebServer server(80);

// Utility function to control LEDs
void setTrafficLights(bool green, bool yellow, bool red) {
  digitalWrite(GREEN_LED, green ? HIGH : LOW);
  digitalWrite(YELLOW_LED, yellow ? HIGH : LOW);
  digitalWrite(RED_LED, red ? HIGH : LOW);
}

// Initialize everything
void setup() {
  Serial.begin(115200);

  // LED pin setup
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  setTrafficLights(false, false, false);

  // WiFi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(1000);
  }
  Serial.println("WiFi connected. IP: " + WiFi.localIP().toString());

  // HTTP GET endpoint to update data
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("vehicles")) {
      vehicleCount = request->getParam("vehicles")->value().toInt();
    }
    if (request->hasParam("ambulance")) {
      ambulanceDetected = request->getParam("ambulance")->value() == "1";
    }
    Serial.printf("Update → Vehicles: %d | Ambulance: %s\n", vehicleCount, ambulanceDetected ? "YES" : "NO");
    request->send(200, "text/plain", "Update received");
  });

  server.begin();
}

// Traffic light logic loop
void loop() {
  if (ambulanceDetected) {
    // Priority green for ambulance
    Serial.println("Ambulance detected — giving priority green");
    setTrafficLights(true, false, false);
    delay(max(vehicleCount * 1000, 7000));  // At least 7 seconds or 1s per vehicle
    ambulanceDetected = false;
  } else if (vehicleCount > 5) {
    // Heavy traffic cycle
    Serial.println("Heavy traffic — executing extended cycle");
    setTrafficLights(true, false, false);
    delay(5000);
    setTrafficLights(false, true, false);
    delay(2000);
    setTrafficLights(false, false, true);
    delay(3000);
  } else {
    // Light traffic cycle
    Serial.println("Light traffic — running normal cycle");
    setTrafficLights(true, false, false);
    delay(3000);
    setTrafficLights(false, true, false);
    delay(1000);
    setTrafficLights(false, false, true);
    delay(3000);
  }
}
