#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Crypto.h>
#include <ChaChaPoly.h>

// Motor driver pins
const int IN1 = D1;  // GPIO5
const int IN2 = D2;  // GPIO4
const int IN3 = D3;  // GPIO14
const int IN4 = D4;  // GPIO12

volatile char lastCommand = 'S'; // Shared variable

// --- SECURITY DEFINITIONS ---
// 256-bit (32 byte) Pre-Shared Key. MUST MATCH ON ESP32.
const uint8_t SHARED_KEY[32] = {
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
  0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
};

// Payload Structure
typedef struct __attribute__((packed)) {
  uint32_t seq;           // Sequence number for replay protection
  uint8_t ciphertext[1];  // Encrypted command ('F' or 'S')
  uint8_t mac[16];        // Authentication tag
} EncryptedPacket;

uint32_t last_seq = 0;    // Store the last received sequence number
ChaChaPoly chachapoly;    // Instantiate the cipher
// ----------------------------


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
  // Ensure the packet is the correct size
  if (len == sizeof(EncryptedPacket)) {
    EncryptedPacket *pkt = (EncryptedPacket *)data;

    // 1. Replay Protection: Drop if sequence number is older or identical
    if (pkt->seq <= last_seq && last_seq != 0) {
      Serial.println("Security Alert: Replay attack detected!");
      return;
    }

    // 2. Reconstruct 12-byte Nonce from the 4-byte sequence number
    uint8_t nonce[12] = {0};
    memcpy(nonce, &pkt->seq, sizeof(pkt->seq));

    // 3. Set up the cipher
    chachapoly.clear();
    chachapoly.setKey(SHARED_KEY, 32);
    chachapoly.setIV(nonce, 12);

    // 4. Decrypt the ciphertext
    uint8_t decrypted_cmd;
    chachapoly.decrypt(&decrypted_cmd, pkt->ciphertext, 1);

    // 5. Verify the MAC (Authentication Tag)
    if (chachapoly.checkTag(pkt->mac, 16)) { 
      // Authentication successful! The data is valid and untampered.
      lastCommand = (char)decrypted_cmd;
      last_seq = pkt->seq; // Update the sequence number
    } else {
      Serial.println("Security Alert: Authentication failed (Bad MAC)!");
    }
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

  Serial.println("ESP8266 SECURE Receiver Ready...");
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

  delay(20);
}
