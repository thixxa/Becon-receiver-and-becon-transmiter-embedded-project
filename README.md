# 📡 LoRa Beacon — Transmitter & Receiver

> An Arduino-based proximity detection system using LoRa radio communication and RSSI-based distance estimation.

---

## 🧭 Overview

This embedded systems project implements a **beacon-based proximity detection system** using two Arduino nodes communicating over LoRa at **433 MHz**. The transmitter periodically broadcasts a beacon signal; the receiver picks it up, estimates the distance from the signal strength (RSSI), and displays it visually via an I²C LCD and a 4-LED indicator bar.

---

## ⚙️ How It Works

```
[ Transmitter Node ]  ──── LoRa 433 MHz ────►  [ Receiver Node ]
   Arduino + LoRa module                          Arduino + LoRa module
   Broadcasts beacon packet                       Reads RSSI from packet
                                                  Calculates distance
                                                  → LCD display
                                                  → LED proximity bar
```

### Distance Estimation (Log-Distance Path Loss Model)

The receiver estimates distance using:

```
d = d₀ × 10^((RSSI₀ - RSSI) / (10 × n))
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `RSSI₀`   | -40 dBm | Reference RSSI at 1 metre |
| `n`       | 2.7     | Path loss exponent |
| `d₀`      | 1.0 m   | Reference distance |

### LED Proximity Indicator

| LEDs ON | Distance | Zone |
|---------|----------|------|
| 🔴 (blinking) | > 60 m | Very Far |
| 🔴🟡 | 40 – 60 m | Far |
| 🔴🟡🟢 | 20 – 40 m | Near |
| 🔴🟡🟢🔵 | < 20 m | Very Near |

---

## 🗂️ Repository Structure

```
├── transmitter/
│   └── corected_transmiter.ino   # Transmitter sketch (LoRa beacon broadcast)
├── reciever/
│   └── corected_receiver.ino     # Receiver sketch (RSSI distance + display)
├── libraries/
│   ├── LoRa/                     # Sandeep Mistry's Arduino LoRa library
│   └── LiquidCrystal_I2C-1.1.2/ # I²C LCD library
└── doc/
    ├── Beacon Receiver and Beacon Transmitter.pdf
    ├── Beacon Receiver and Beacon Transmitter.pptx
    └── EC6020_Beacon Receiver and Beacon Transmitter_Project Proposal.pdf
```

---

## 🛠️ Hardware Requirements

### Per Node
- Arduino Uno / Nano
- LoRa SX1278 module (433 MHz)

### Receiver Only
- 16×2 I²C LCD (address `0x27`)
- 4× LEDs + current-limiting resistors
  - LED1 → Pin 5
  - LED2 → Pin 6
  - LED3 → Pin 7
  - LED4 → Pin 8

### Wiring (LoRa ↔ Arduino)
| LoRa Pin | Arduino Pin |
|----------|-------------|
| SCK      | 13          |
| MISO     | 12          |
| MOSI     | 11          |
| NSS/CS   | 10          |
| RST      | 9           |
| DIO0     | 2           |

---

## 🚀 Getting Started

### 1. Install Libraries
Copy the folders from `libraries/` into your Arduino libraries directory, or install via Arduino Library Manager:
- `LoRa` by Sandeep Mistry
- `LiquidCrystal_I2C` by Frank de Brabander

### 2. Flash the Transmitter
Open `transmitter/corected_transmiter.ino` in Arduino IDE and upload to your transmitter Arduino.

### 3. Flash the Receiver
Open `reciever/corected_receiver.ino` in Arduino IDE and upload to your receiver Arduino.

### 4. Power Both Nodes
The LCD will show **"LoRa Receiver"** and then **"Waiting..."** until a packet is received. Once the transmitter is in range, the LCD will display the estimated distance and the LEDs will light up accordingly.

---

## 📡 LoRa Configuration

Both nodes use the following settings (defaults from the library):

| Setting | Value |
|---------|-------|
| Frequency | 433 MHz |
| Bandwidth | 125 kHz |
| Spreading Factor | SF7 |
| Coding Rate | 4/5 |

---

## 📚 Documentation

Full project report and presentation slides are in the `doc/` folder.

---

## 🧑‍💻 Built With

- [Arduino IDE](https://www.arduino.cc/en/software)
- [Arduino LoRa library](https://github.com/sandeepmistry/arduino-LoRa) — Sandeep Mistry
- [LiquidCrystal_I2C](https://github.com/johnrickman/LiquidCrystal_I2C)

---

*EC6020 Embedded Systems Mini Project*
