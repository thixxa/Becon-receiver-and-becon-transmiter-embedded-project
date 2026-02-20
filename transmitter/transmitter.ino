#include <SPI.h>
#include <LoRa.h>

#define SWITCH_PIN 7     // Switch connected to GND
#define LED_PIN 4        // LED pin

int counter = 0;
const unsigned long PACKET_GAP = 5000; // 5 seconds

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Sender (Switch Controlled)");

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop() {

  if (digitalRead(SWITCH_PIN) == LOW) {   // Switch ON

    Serial.print("Sending packet: ");
    Serial.println(counter);

    // ---- LED BLINK ONCE (TX indication) ----
    digitalWrite(LED_PIN, HIGH);
    delay(200);               // LED ON time (blink)
    digitalWrite(LED_PIN, LOW);

    // ---- Send packet ----
    LoRa.beginPacket();
    LoRa.print("hello ");
    LoRa.print(counter);
    LoRa.endPacket();

    counter++;

    // ---- 5 second gap (LED OFF) ----
    delay(PACKET_GAP);
  }
  else {
    digitalWrite(LED_PIN, LOW); // LED OFF
    delay(100);
  }
}