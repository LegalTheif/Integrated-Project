#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// WiFi credentials
const char* ssid = "OPPO A52";
const char* password = "opnumber";

AsyncWebServer server(80);

// Pins for traffic lights
const int greenPin = 5;   // D1 → GPIO5
const int yellowPin = 4;  // D2 → GPIO4
const int redPin = 0;     // D3 → GPIO0

volatile int vehicleCount = 0;
volatile bool ambulanceDetected = false;

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi!");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(greenPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(redPin, OUTPUT);

  connectToWiFi();

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("vehicles")) {
      vehicleCount = request->getParam("vehicles")->value().toInt();
    }
    if (request->hasParam("ambulance")) {
      ambulanceDetected = request->getParam("ambulance")->value() == "1";
    }
    Serial.printf("Received Update → Vehicles: %d | Ambulance: %s\n", vehicleCount, ambulanceDetected ? "YES" : "NO");
    request->send(200, "text/plain", "Update received");
  });

  server.begin();
}

void loop() {
  if (ambulanceDetected) {
    int holdTime = max(vehicleCount * 1000, 7000);  // at least 7 sec for ambulance
    Serial.printf("🚑 Ambulance detected! Holding GREEN for %d ms\n", holdTime);

    digitalWrite(greenPin, HIGH);
    digitalWrite(yellowPin, LOW);
    digitalWrite(redPin, LOW);
    delay(holdTime);

    ambulanceDetected = false;  // reset flag after priority pass

  } else if (vehicleCount > 5) {
    Serial.println("🚗 Heavy traffic mode");

    digitalWrite(greenPin, HIGH);
    delay(5000);

    digitalWrite(greenPin, LOW);
    digitalWrite(yellowPin, HIGH);
    delay(2000);

    digitalWrite(yellowPin, LOW);
    digitalWrite(redPin, HIGH);
    delay(3000);

  } else {
    Serial.println("🚦 Light traffic mode");
  digitalWrite(greenPin, HIGH);
  digitalWrite(yellowPin, LOW);
  digitalWrite(redPin, LOW);
  delay(3000);

  digitalWrite(greenPin, LOW);
  digitalWrite(yellowPin, HIGH);
  digitalWrite(redPin, LOW);
  delay(1000);

  digitalWrite(greenPin, LOW);
  digitalWrite(yellowPin, LOW);
  digitalWrite(redPin, HIGH);
  delay(3000);

  }
}
