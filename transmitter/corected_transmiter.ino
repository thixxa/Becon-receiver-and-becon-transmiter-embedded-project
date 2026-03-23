#include <SPI.h>
#include <LoRa.h>
#include "DHT.h"

#define START_SWITCH 7
#define RESET_SWITCH 5
#define VIBRATION_PIN 6
#define LED_PIN 4
#define DHTPIN 3
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

int counter = 0;
const unsigned long PACKET_GAP = 5000;

bool transmitMode = false;
int lastVibrationState = LOW;
int lastResetState = HIGH;

float lastHumidity = 0;
bool firstRead = true;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Sender (Advanced Reset Logic)");

  pinMode(START_SWITCH, INPUT_PULLUP);
  pinMode(RESET_SWITCH, INPUT_PULLUP);
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  dht.begin();

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop() {

  int resetState = digitalRead(RESET_SWITCH);

  // -------- RESET LOW → System Disabled --------
  if (resetState == LOW) {
    transmitMode = false;
    digitalWrite(LED_PIN, LOW);
  }

  // -------- Detect RESET rising edge (0 → 1) --------
  if (lastResetState == LOW && resetState == HIGH) {
    Serial.println("System Reset Complete - Waiting...");
    transmitMode = false;
    counter = 0;
    firstRead = true;   // reinitialize humidity baseline
  }

  lastResetState = resetState;

  // -------- Only run system if RESET is HIGH --------
  if (resetState == HIGH) {

    int startState = digitalRead(START_SWITCH);
    int vibrationState = digitalRead(VIBRATION_PIN);
    float humidity = dht.readHumidity();

    // START SWITCH
    if (startState == LOW) {
      transmitMode = true;
    }

    // VIBRATION (single trigger)
    if (vibrationState == HIGH && lastVibrationState == LOW) {
      transmitMode = true;
    }
    lastVibrationState = vibrationState;

    // HUMIDITY INCREASE >10%
    if (!isnan(humidity)) {

      if (firstRead) {
        lastHumidity = humidity;
        firstRead = false;
      }

      float increase = humidity - lastHumidity;

      if (increase > 10.0) {
        Serial.println("Humidity Increased >10%");
        transmitMode = true;
        lastHumidity = humidity;
      }
    }

    // CONTINUOUS TRANSMISSION
    if (transmitMode) {

      Serial.print("Sending packet: ");
      Serial.print(counter);
      Serial.print(" | Humidity: ");
      Serial.println(humidity);

      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);

      LoRa.beginPacket();
      LoRa.print("hello ");
      LoRa.print(counter);
      LoRa.print(" Humidity: ");
      LoRa.print(humidity);
      LoRa.endPacket();

      counter++;

      delay(PACKET_GAP);
    }
  }
}