// Samples per second
#define SAMPLE_RATE 500

// Make sure to set the same baud rate on your Serial Monitor/Plotter
#define BAUD_RATE 115200

// Change if not using A0 analog pin
#define INPUT_PIN 33

#include <esp_now.h>
#include <WiFi.h>
#include <Crypto.h>
#include <ChaChaPoly.h> // Using consistent library across both devices

// Receiver MAC address
uint8_t receiverMAC[] = {0x2C, 0xF4, 0x32, 0x2D, 0xB6, 0x53};

// --- SECURITY DEFINITIONS ---
// 256-bit (32 byte) Pre-Shared Key. MUST MATCH ON BOTH DEVICES.
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

uint32_t sequence_number = 0; // Monotonically increasing counter
ChaChaPoly chachapoly;        // Instantiate the cipher
// ----------------------------

// NEW ESPNOW Callback format for ESP32 Core v3.x
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");

  Serial.print("TX Status Code: ");
  Serial.println(info->tx_status);
}

void setup() {
  // Serial connection begin
  Serial.begin(BAUD_RATE);

   WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed!");
    return;
  }

  // Register new-style callback
  esp_now_register_send_cb(onSent);
  
  // Add peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false; // We are doing app-layer encryption instead
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer!");
    return;
  }
}

// Function to send encrypted command
void sendEncryptedCommand(char cmd) {
  EncryptedPacket pkt;
  pkt.seq = ++sequence_number;

  // Generate 12-byte nonce from the sequence number
  uint8_t nonce[12] = {0};
  memcpy(nonce, &pkt.seq, sizeof(pkt.seq));

  // Set up the cipher
  chachapoly.clear();
  chachapoly.setKey(SHARED_KEY, 32);
  chachapoly.setIV(nonce, 12);

  // Encrypt the command into the ciphertext buffer
  chachapoly.encrypt(pkt.ciphertext, (uint8_t*)&cmd, 1);
  
  // Compute the Authentication Tag (MAC)
  chachapoly.computeTag(pkt.mac, 16);

  esp_now_send(receiverMAC, (uint8_t*)&pkt, sizeof(pkt));
}

void loop()
{
  // Calculate elapsed time
  static unsigned long past = 0;
  unsigned long present = micros();
  unsigned long interval = present - past;
  past = present;

  // Run timer
  static long timer = 0;
  timer -= interval;

  // Sample
  if (timer < 0)
  {
    timer += 1000000 / SAMPLE_RATE;
    
    // Get analog input value (Raw EMG)
    float sensor_value = analogRead(INPUT_PIN);
    
    // Apply the band-stop filter (48 Hz to 52 Hz)
    float bandstop_filtered = BandStopFilter(sensor_value);
    
    // Apply the high-pass filter (70 Hz)
    float highpass_filtered = HighPassFilter(bandstop_filtered);
    
    // Print the final filtered signal
    Serial.println(highpass_filtered);
    
    if (highpass_filtered < -200 || highpass_filtered > 200){
      sendEncryptedCommand('F');
      delay(100);
    }
    else {
      sendEncryptedCommand('S');
      delay(100);
    }
  }
}

// High-Pass Butterworth IIR digital filter
float HighPassFilter(float input){
  float output = input;
  {
    static float z1, z2;
    float x = output - -0.82523238 * z1 - 0.29463653 * z2;
    output = 0.52996723 * x + -1.05993445 * z1 + 0.52996723 * z2;
    z2 = z1;
    z1 = x;
  }
  return output;
}

// Band-Stop Butterworth IIR digital filter
float BandStopFilter(float input)
{
  float output = input;
  {
    static float z1, z2;
    float x = output - -1.56858163 * z1 - 0.96424138 * z2;
    output = 0.96508099 * x + -1.56202714 * z1 + 0.96508099 * z2;
    z2 = z1;
    z1 = x;
  }
  {
    static float z1, z2;
    float x = output - -1.61100358 * z1 - 0.96592171 * z2;
    output = 1.00000000 * x + -1.61854514 * z1 + 1.00000000 * z2;
    z2 = z1;
    z1 = x;
  }
  return output;
}
