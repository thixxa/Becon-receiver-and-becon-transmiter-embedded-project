#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

// ---------------- PIN DEFINITIONS ----------------
#define START_SWITCH   PD7
#define RESET_SWITCH   PD5
#define VIBRATION_PIN  PD6
#define LED_PIN        PD4
#define DHTPIN         PD3

// SPI Pins (ATmega328P) used for LoRa SX1278
#define SS   PB2        // Slave Select(select LoRa)
#define MOSI PB3        // Master Out Slave In (send data from microcontroller to LoRa)
#define MISO PB4        // Master In Slave Out (receive data from LoRa, not used in this project)
#define SCK  PB5        // Serial Clock (clock signal for synchronization)

// ---------------- GLOBAL VARIABLES ----------------
int counter = 0;                    // counts sent packets
uint8_t transmitMode = 0;           // decides whether to send data
uint8_t lastVibrationState = 0;     // detect edge
uint8_t lastResetState = 1;         // detect reset release

float lastHumidity = 0;             // compare changes
uint8_t firstRead = 1;              // initialize first value

// ---------------- UART (for debugging) ----------------
void UART_init() {
    UBRR0H = 0;
    UBRR0L = 103;                   // 9600 baud
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART_sendChar(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void UART_print(char *str) {
    while (*str) UART_sendChar(*str++);
}

// ---------------- SPI ----------------
void SPI_init() {
    DDRB |= (1 << MOSI) | (1 << SCK) | (1 << SS);
    DDRB &= ~(1 << MISO);

    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
}

// sends and receives 1 byte over SPI
uint8_t SPI_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)));
    return SPDR;
}

// ---------------- LoRa (Basic SX1278 Write) ----------------
// write data into LoRa module registers
void LoRa_writeRegister(uint8_t addr, uint8_t value) {
    PORTB &= ~(1 << SS);
    SPI_transfer(addr | 0x80);
    SPI_transfer(value);
    PORTB |= (1 << SS);
}

// VERY simplified packet send
void LoRa_sendPacket(char *data) {
    // Set FIFO pointer
    LoRa_writeRegister(0x0D, 0x00);

    // Write payload
    for (int i = 0; i < strlen(data); i++) {
        LoRa_writeRegister(0x00, data[i]);
    }

    // Payload length
    LoRa_writeRegister(0x22, strlen(data));

    // Set TX mode
    LoRa_writeRegister(0x01, 0x83);

    _delay_ms(100);
}

// ---------------- DHT11 humidity sensor reading ---------------
uint8_t DHT_read(float *humidity) {
    uint8_t data[5] = {0};

    // Request
    DDRD |= (1 << DHTPIN);
    PORTD &= ~(1 << DHTPIN);
    _delay_ms(18);
    PORTD |= (1 << DHTPIN);
    DDRD &= ~(1 << DHTPIN);

    _delay_us(40);

    if (PIND & (1 << DHTPIN)) return 0;

    while (!(PIND & (1 << DHTPIN)));
    while (PIND & (1 << DHTPIN));

    // Read 40 bits
    for (int i = 0; i < 40; i++) {
        while (!(PIND & (1 << DHTPIN)));
        _delay_us(30);

        if (PIND & (1 << DHTPIN))
            data[i / 8] |= (1 << (7 - (i % 8)));

        while (PIND & (1 << DHTPIN));
    }

    *humidity = data[0];                // only integer humidity is used
    return 1;
}

// ---------------- MAIN ----------------
int main(void) {

    UART_init();
    SPI_init();

    // Pin setup
    DDRD &= ~((1 << START_SWITCH) | (1 << RESET_SWITCH) | (1 << VIBRATION_PIN));
    PORTD |= (1 << START_SWITCH) | (1 << RESET_SWITCH); // pull-up

    DDRD |= (1 << LED_PIN);

    while (1) {

        uint8_t resetState = (PIND & (1 << RESET_SWITCH)) ? 1 : 0;

        // when reset button pressed
        if (resetState == 0) {
            transmitMode = 0;               // stop transmitting
            PORTD &= ~(1 << LED_PIN);       // turn off LED
        }

        // when reset button released
        if (lastResetState == 0 && resetState == 1) {
            UART_print("System Reset Complete\n");
            transmitMode = 0;               // reset state
            counter = 0;                    // reset packet counter
            firstRead = 1;
        }

        lastResetState = resetState;

        if (resetState == 1) {

            uint8_t startState = (PIND & (1 << START_SWITCH)) ? 1 : 0;
            uint8_t vibrationState = (PIND & (1 << VIBRATION_PIN)) ? 1 : 0;

            float humidity = 0;

            // manual start
            if (startState == 0) {
                transmitMode = 1;
            }

            // trigger on vibration detected (rising edge)
            if (vibrationState == 1 && lastVibrationState == 0) {
                transmitMode = 1;
            }
            lastVibrationState = vibrationState;

            // DHT read
            if (DHT_read(&humidity)) {

                if (firstRead) {
                    lastHumidity = humidity;
                    firstRead = 0;
                }

                float increase = humidity - lastHumidity;
                
                // if humidity increased by more than 10% since last read, start transmission
                if (increase > 10.0) {
                    UART_print("Humidity Increased >10%\n");
                    transmitMode = 1;
                    lastHumidity = humidity;
                }
            }

            // TRANSMIT
            if (transmitMode) {

                char buffer[64];
                sprintf(buffer, "hello %d Humidity: %.1f", counter, humidity);

                UART_print(buffer);
                UART_print("\n");

                PORTD |= (1 << LED_PIN);
                _delay_ms(200);
                PORTD &= ~(1 << LED_PIN);

                LoRa_sendPacket(buffer);

                counter++;
                _delay_ms(5000);
            }
        }
    }
}