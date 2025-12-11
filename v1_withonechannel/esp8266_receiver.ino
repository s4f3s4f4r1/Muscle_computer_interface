#include <ESP8266WiFi.h>
#include <espnow.h>

// Motor driver pins
const int IN1 = D1;  // GPIO5
const int IN2 = D2;  // GPIO4
const int IN3 = D3;  // GPIO14
const int IN4 = D4;  // GPIO12

volatile char lastCommand = 'S';   // Shared variable

void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ---------- ESP-NOW CALLBACK ----------
void onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len > 0) {
    lastCommand = (char)data[0];   // Store command only
  }
}
// --------------------------------------

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopMotor();

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init Failed!");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceive);

  Serial.println("ESP8266 Ready...");
}

void loop() {

  if (lastCommand == 'F') {
    Serial.println("FORWARD");
    forward();
    delay(500);
  }
  else if (lastCommand == 'S') {
    Serial.println("STOP");
    stopMotor();
  }

  delay(20);   // safe in loop, NOT in callback
}
