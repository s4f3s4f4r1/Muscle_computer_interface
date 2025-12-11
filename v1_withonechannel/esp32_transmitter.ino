// Samples per second
#define SAMPLE_RATE 500

// Make sure to set the same baud rate on your Serial Monitor/Plotter
#define BAUD_RATE 115200

// Change if not using A0 analog pin
#define INPUT_PIN 33

#include <esp_now.h>
#include <WiFi.h>

// Receiver MAC address
uint8_t receiverMAC[] = {0x2C, 0xF4, 0x32, 0x2D, 0xB6, 0x53};

// NEW ESPNOW Callback format for ESP32 Core v3.x
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");

  Serial.print("TX Status Code: ");
  Serial.println(info->tx_status);  // NEW FIELD
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
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer!");
    return;
  }
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
     if (highpass_filtered< -200 || highpass_filtered> 200){
            const char *msg = "F";
            //Serial.println(highpass_filtered);
            esp_now_send(receiverMAC, (uint8_t*)msg, strlen(msg));
             delay(100);
             
         }
    else{
         const char *msg = "S";
         esp_now_send(receiverMAC, (uint8_t*)msg, strlen(msg));
         delay(100);
    }
  }
}

// High-Pass Butterworth IIR digital filter, generated using filter_gen.py.
// Sampling rate: 500.0 Hz, frequency: 70.0 Hz.
// Filter is order 2, implemented as second-order sections (biquads).
// Reference: https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.butter.html
float HighPassFilter(float input){
  float output = input;
  {
    static float z1, z2; // filter section state
    float x = output - -0.82523238 * z1 - 0.29463653 * z2;
    output = 0.52996723 * x + -1.05993445 * z1 + 0.52996723 * z2;
    z2 = z1;
    z1 = x;
  }
  return output;
}

// Band-Stop Butterworth IIR digital filter, generated using filter_gen.py.
// Sampling rate: 500.0 Hz, frequency: [48.0, 52.0] Hz.
// Filter is order 2, implemented as second-order sections (biquads).
// Reference: https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.butter.html
float BandStopFilter(float input)
{
  float output = input;
  {
    static float z1, z2; // filter section state
    float x = output - -1.56858163 * z1 - 0.96424138 * z2;
    output = 0.96508099 * x + -1.56202714 * z1 + 0.96508099 * z2;
    z2 = z1;
    z1 = x;
  }
  {
    static float z1, z2; // filter section state
    float x = output - -1.61100358 * z1 - 0.96592171 * z2;
    output = 1.00000000 * x + -1.61854514 * z1 + 1.00000000 * z2;
    z2 = z1;
    z1 = x;
  }
  return output;
}
