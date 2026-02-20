#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -------- LCD --------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------- LED Pins --------
#define LED1 5
#define LED2 6
#define LED3 7
#define LED4 8

// -------- Distance model parameters --------
float RSSI_0 = -40.0;   // RSSI at 1 meter
float n = 2.7;          // Path loss exponent
float d0 = 1.0;

// -------- Distance calculation --------
float calculateDistance(int rssi) {
  return d0 * pow(10.0, (RSSI_0 - rssi) / (10.0 * n));
}

// -------- Turn OFF all LEDs --------
void allLEDsOff() {
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
}

void setup() {
  Serial.begin(9600);

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  allLEDsOff();

  // LCD
  Wire.begin();   // SDA=A4, SCL=A5
  lcd.init();
  lcd.backlight();

  lcd.print("LoRa Receiver");

  // LoRa
  if (!LoRa.begin(433E6)) {
    lcd.setCursor(0, 1);
    lcd.print("LoRa Failed");
    while (1);
  }

  lcd.setCursor(0, 1);
  lcd.print("Waiting...");
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    while (LoRa.available()) {
      LoRa.read();
    }

    int rssi = LoRa.packetRssi();
    float distance = calculateDistance(rssi);

    // ---- Serial ----
    Serial.print("Distance: ");
    Serial.print(distance, 2);
    Serial.println(" m");

    // ---- LCD ----
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Distance:");
    lcd.setCursor(0, 1);
    lcd.print(distance, 1);
    lcd.print(" m");

    // ---- LED LOGIC (CUMULATIVE) ----
    allLEDsOff();

    if (distance > 60) {               // VERY FAR
      digitalWrite(LED1, HIGH);
      delay(300);
      digitalWrite(LED1, LOW);
      delay(300);
    }
    else if (distance > 40) {          // FAR
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
    }
    else if (distance > 20) {          // NEAR
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
      digitalWrite(LED3, HIGH);
    }
    else {                             // VERY NEAR
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
      digitalWrite(LED3, HIGH);
      digitalWrite(LED4, HIGH);
    }
  }
}