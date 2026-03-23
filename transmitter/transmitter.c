/*
 * transmitter.c — LoRa Transmitter (Switch Controlled)
 * Target: ATmega328P @ 16 MHz
 * LoRa module: SX1278 (RFM95) at 433 MHz via SPI
 *
 * Pin Mapping (Arduino Uno → ATmega328P):
 *   SWITCH_PIN  = D7  → PD7
 *   SPI MOSI    = D11 → PB3
 *   SPI MISO    = D12 → PB4
 *   SPI SCK     = D13 → PB5
 *   LoRa NSS    = D10 → PB2
 *   LoRa RESET  = D9  → PB1
 *   LoRa DIO0   = D2  → PD2
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ─────────────────────────────────────────────
   UART  (9600 baud @ 16 MHz)
───────────────────────────────────────────── */
#define BAUD 9600
#define UBRR_VAL ((F_CPU / (16UL * BAUD)) - 1)

static void uart_init(void) {
    UBRR0H = (uint8_t)(UBRR_VAL >> 8);
    UBRR0L = (uint8_t)(UBRR_VAL);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); /* 8N1 */
}

static void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

static void uart_putint(int16_t val) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", val);
    uart_puts(buf);
}

/* ─────────────────────────────────────────────
   SPI
───────────────────────────────────────────── */
/* NSS = PB2, SCK = PB5, MOSI = PB3, MISO = PB4 */
#define LORA_NSS_DDR   DDRB
#define LORA_NSS_PORT  PORTB
#define LORA_NSS_PIN   PB2

#define LORA_RST_DDR   DDRB
#define LORA_RST_PORT  PORTB
#define LORA_RST_PIN   PB1

static void spi_init(void) {
    /* MOSI, SCK, NSS, RST as outputs */
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << LORA_NSS_PIN) | (1 << LORA_RST_PIN);
    DDRB &= ~(1 << PB4); /* MISO input */
    LORA_NSS_PORT |= (1 << LORA_NSS_PIN); /* NSS high */
    /* SPI Master, Mode 0, fosc/4 */
    SPCR = (1 << SPE) | (1 << MSTR);
}

static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)));
    return SPDR;
}

/* ─────────────────────────────────────────────
   SX1278 / LoRa Register Definitions
───────────────────────────────────────────── */
#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_FIFO_TX_BASE_ADDR    0x0E
#define REG_FIFO_RX_BASE_ADDR    0x0F
#define REG_IRQ_FLAGS            0x12
#define REG_PAYLOAD_LENGTH       0x22
#define REG_MODEM_CONFIG_1       0x1D
#define REG_MODEM_CONFIG_2       0x1E
#define REG_SYNC_WORD            0x39
#define REG_DIO_MAPPING_1        0x40
#define REG_VERSION              0x42

#define MODE_LONG_RANGE_MODE     0x80
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define MODE_RX_CONTINUOUS       0x05

#define PA_BOOST                 0x80

#define IRQ_TX_DONE_MASK         0x08

static uint8_t lora_read_reg(uint8_t addr) {
    LORA_NSS_PORT &= ~(1 << LORA_NSS_PIN);
    spi_transfer(addr & 0x7F);
    uint8_t val = spi_transfer(0x00);
    LORA_NSS_PORT |= (1 << LORA_NSS_PIN);
    return val;
}

static void lora_write_reg(uint8_t addr, uint8_t val) {
    LORA_NSS_PORT &= ~(1 << LORA_NSS_PIN);
    spi_transfer(addr | 0x80);
    spi_transfer(val);
    LORA_NSS_PORT |= (1 << LORA_NSS_PIN);
}

static uint8_t lora_begin(void) {
    /* Hardware reset */
    LORA_RST_PORT &= ~(1 << LORA_RST_PIN);
    _delay_ms(10);
    LORA_RST_PORT |= (1 << LORA_RST_PIN);
    _delay_ms(10);

    /* Check version */
    uint8_t version = lora_read_reg(REG_VERSION);
    if (version != 0x12) return 0; /* SX1278 not found */

    /* Sleep → LoRa mode */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
    _delay_ms(10);

    /* Set frequency: 433 MHz
       Fstep = 32e6 / 2^19 = 61.035 Hz
       FRF   = 433e6 / 61.035 = 7094272 = 0x6C8000 */
    lora_write_reg(REG_FRF_MSB, 0x6C);
    lora_write_reg(REG_FRF_MID, 0x80);
    lora_write_reg(REG_FRF_LSB, 0x00);

    /* PA: +17 dBm via PA_BOOST */
    lora_write_reg(REG_PA_CONFIG, PA_BOOST | 0x0F);

    /* BW=125kHz, CR=4/5, Implicit header off */
    lora_write_reg(REG_MODEM_CONFIG_1, 0x72);
    /* SF=7, CRC on */
    lora_write_reg(REG_MODEM_CONFIG_2, 0x74);

    /* Sync word 0x12 (public network) */
    lora_write_reg(REG_SYNC_WORD, 0x12);

    /* Base addresses */
    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);

    /* Standby */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
    return 1;
}

static void lora_send_packet(const uint8_t *data, uint8_t len) {
    /* Standby */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

    /* Reset FIFO pointer */
    lora_write_reg(REG_FIFO_ADDR_PTR, 0x00);

    /* Write payload */
    for (uint8_t i = 0; i < len; i++) {
        lora_write_reg(REG_FIFO, data[i]);
    }
    lora_write_reg(REG_PAYLOAD_LENGTH, len);

    /* TX mode */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

    /* Wait for TX done */
    while ((lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0);

    /* Clear IRQ */
    lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
}

/* ─────────────────────────────────────────────
   Switch — PD7 (D7), active LOW
───────────────────────────────────────────── */
#define SWITCH_DDR   DDRD
#define SWITCH_PORT  PORTD
#define SWITCH_PIN_R PIND
#define SWITCH_BIT   PD7

/* ─────────────────────────────────────────────
   Main
───────────────────────────────────────────── */
int main(void) {
    uart_init();
    spi_init();

    /* Switch: input with pull-up */
    SWITCH_DDR  &= ~(1 << SWITCH_BIT);
    SWITCH_PORT |=  (1 << SWITCH_BIT);

    uart_puts("LoRa Sender (Switch Controlled)\r\n");

    if (!lora_begin()) {
        uart_puts("Starting LoRa failed!\r\n");
        while (1);
    }

    uint16_t counter = 0;

    while (1) {
        if (!(SWITCH_PIN_R & (1 << SWITCH_BIT))) { /* Switch ON (LOW) */
            uart_puts("Sending packet: ");
            uart_putint((int16_t)counter);
            uart_puts("\r\n");

            /* Build payload: "hello <counter>" */
            char payload[24];
            snprintf(payload, sizeof(payload), "hello %u", counter);
            lora_send_packet((uint8_t *)payload, (uint8_t)strlen(payload));

            counter++;
            _delay_ms(5000);
        } else {
            _delay_ms(100);
        }
    }
}
