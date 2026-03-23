/*
 * receiver.c — LoRa Receiver with I2C LCD and LED distance indicators
 * Target: ATmega328P @ 16 MHz
 * LoRa module: SX1278 (RFM95) at 433 MHz via SPI
 * LCD: 16×2 I2C LCD at address 0x27 via TWI (PCF8574 backpack)
 *
 * Pin Mapping (Arduino Uno → ATmega328P):
 *   LED1  = D5  → PD5
 *   LED2  = D6  → PD6
 *   LED3  = D7  → PD7
 *   LED4  = D8  → PB0
 *   I2C SDA     → PC4  (A4)
 *   I2C SCL     → PC5  (A5)
 *   SPI MOSI    = D11 → PB3
 *   SPI MISO    = D12 → PB4
 *   SPI SCK     = D13 → PB5
 *   LoRa NSS    = D10 → PB2
 *   LoRa RESET  = D9  → PB1
 *   LoRa DIO0   = D2  → PD2
 */

#include <avr/io.h>
#include <util/delay.h>
#include <util/twi.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ─────────────────────────────────────────────
   UART  (9600 baud @ 16 MHz)
───────────────────────────────────────────── */
#define BAUD 9600
#define UBRR_VAL ((F_CPU / (16UL * BAUD)) - 1)

static void uart_init(void) {
    UBRR0H = (uint8_t)(UBRR_VAL >> 8);
    UBRR0L = (uint8_t)(UBRR_VAL);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

/* ─────────────────────────────────────────────
   TWI / I2C  (100 kHz)
───────────────────────────────────────────── */
#define TWI_FREQ 100000UL

static void twi_init(void) {
    TWSR = 0x00;                          /* prescaler = 1 */
    TWBR = (uint8_t)((F_CPU / TWI_FREQ - 16) / 2);
}

static uint8_t twi_start(uint8_t addr_rw) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    TWDR = addr_rw;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    return (TWSR & 0xF8);
}

static void twi_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

static void twi_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    while (TWCR & (1 << TWSTO));
}

/* ─────────────────────────────────────────────
   PCF8574 I2C LCD (16×2, address 0x27)
   Bit layout: [7:4] = data nibble  [3] = BL  [2] = EN  [1] = RW  [0] = RS
───────────────────────────────────────────── */
#define LCD_ADDR 0x27
#define LCD_BL   0x08
#define LCD_EN   0x04
#define LCD_RW   0x02
#define LCD_RS   0x01

static uint8_t lcd_backlight = LCD_BL;

static void lcd_send_byte(uint8_t data) {
    twi_start((LCD_ADDR << 1));
    twi_write(data);
    twi_stop();
}

static void lcd_pulse_enable(uint8_t data) {
    lcd_send_byte(data | LCD_EN);
    _delay_us(1);
    lcd_send_byte(data & ~LCD_EN);
    _delay_us(50);
}

static void lcd_send_nibble(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | lcd_backlight | mode;
    lcd_pulse_enable(data);
}

static void lcd_cmd(uint8_t cmd) {
    lcd_send_nibble(cmd & 0xF0, 0);
    lcd_send_nibble((cmd << 4) & 0xF0, 0);
    _delay_us(37);
}

static void lcd_data(uint8_t ch) {
    lcd_send_nibble(ch & 0xF0, LCD_RS);
    lcd_send_nibble((ch << 4) & 0xF0, LCD_RS);
    _delay_us(37);
}

static void lcd_init(void) {
    _delay_ms(50);
    /* 4-bit initialisation sequence */
    lcd_send_nibble(0x30, 0); _delay_ms(5);
    lcd_send_nibble(0x30, 0); _delay_us(100);
    lcd_send_nibble(0x30, 0); _delay_us(100);
    lcd_send_nibble(0x20, 0); /* set 4-bit mode */

    lcd_cmd(0x28); /* 2 lines, 5×8 font */
    lcd_cmd(0x0C); /* display on, cursor off */
    lcd_cmd(0x06); /* entry mode: increment, no shift */
    lcd_cmd(0x01); /* clear display */
    _delay_ms(2);
}

static void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t offsets[] = {0x00, 0x40};
    lcd_cmd(0x80 | (col + offsets[row]));
}

static void lcd_print(const char *s) {
    while (*s) lcd_data((uint8_t)*s++);
}

static void lcd_clear(void) {
    lcd_cmd(0x01);
    _delay_ms(2);
}

/* ─────────────────────────────────────────────
   SPI
───────────────────────────────────────────── */
#define LORA_NSS_PORT  PORTB
#define LORA_NSS_PIN   PB2
#define LORA_RST_PORT  PORTB
#define LORA_RST_PIN   PB1

static void spi_init(void) {
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << LORA_NSS_PIN) | (1 << LORA_RST_PIN);
    DDRB &= ~(1 << PB4);
    LORA_NSS_PORT |= (1 << LORA_NSS_PIN);
    SPCR = (1 << SPE) | (1 << MSTR);
}

static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)));
    return SPDR;
}

/* ─────────────────────────────────────────────
   SX1278 / LoRa Registers
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
#define REG_RX_NB_BYTES          0x13
#define REG_PKT_SNR_VALUE        0x19
#define REG_PKT_RSSI_VALUE       0x1A
#define REG_MODEM_CONFIG_1       0x1D
#define REG_MODEM_CONFIG_2       0x1E
#define REG_SYNC_WORD            0x39
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS            0x12
#define REG_VERSION              0x42

#define MODE_LONG_RANGE_MODE     0x80
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_RX_CONTINUOUS       0x05

#define PA_BOOST                 0x80

#define IRQ_RX_DONE_MASK         0x40

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
    LORA_RST_PORT &= ~(1 << LORA_RST_PIN);
    _delay_ms(10);
    LORA_RST_PORT |= (1 << LORA_RST_PIN);
    _delay_ms(10);

    if (lora_read_reg(REG_VERSION) != 0x12) return 0;

    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
    _delay_ms(10);

    /* 433 MHz */
    lora_write_reg(REG_FRF_MSB, 0x6C);
    lora_write_reg(REG_FRF_MID, 0x80);
    lora_write_reg(REG_FRF_LSB, 0x00);

    lora_write_reg(REG_PA_CONFIG, PA_BOOST | 0x0F);
    lora_write_reg(REG_MODEM_CONFIG_1, 0x72); /* BW=125k, CR=4/5 */
    lora_write_reg(REG_MODEM_CONFIG_2, 0x74); /* SF=7, CRC on */
    lora_write_reg(REG_SYNC_WORD, 0x12);

    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);

    /* RX Continuous */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
    return 1;
}

/*
 * Returns 1 if a packet is ready, 0 otherwise.
 * Drains the FIFO (discards payload — only RSSI is used).
 */
static uint8_t lora_parse_packet(void) {
    uint8_t irq = lora_read_reg(REG_IRQ_FLAGS);
    if (!(irq & IRQ_RX_DONE_MASK)) return 0;

    /* Clear IRQ flags */
    lora_write_reg(REG_IRQ_FLAGS, irq);

    /* Drain FIFO */
    uint8_t nb = lora_read_reg(REG_RX_NB_BYTES);
    uint8_t rx_addr = lora_read_reg(REG_FIFO_RX_CURRENT_ADDR);
    lora_write_reg(REG_FIFO_ADDR_PTR, rx_addr);
    for (uint8_t i = 0; i < nb; i++) lora_read_reg(REG_FIFO);

    return 1;
}

/*
 * Returns RSSI in dBm for the last received packet.
 * Formula: RSSI = -157 + REG_PKT_RSSI_VALUE  (for LF port / 433 MHz on SX1278)
 */
static int16_t lora_packet_rssi(void) {
    return (int16_t)lora_read_reg(REG_PKT_RSSI_VALUE) - 157;
}

/* ─────────────────────────────────────────────
   Distance model
   distance = d0 * 10 ^ ((RSSI_0 − RSSI) / (10 * n))
───────────────────────────────────────────── */
static float calculate_distance(int16_t rssi) {
    float rssi_0 = -40.0f;
    float n      =   2.7f;
    float d0     =   1.0f;
    return d0 * powf(10.0f, (rssi_0 - (float)rssi) / (10.0f * n));
}

/* ─────────────────────────────────────────────
   LEDs
   LED1 = PD5, LED2 = PD6, LED3 = PD7, LED4 = PB0
───────────────────────────────────────────── */
static void leds_init(void) {
    DDRD |= (1 << PD5) | (1 << PD6) | (1 << PD7);
    DDRB |= (1 << PB0);
}

static void all_leds_off(void) {
    PORTD &= ~((1 << PD5) | (1 << PD6) | (1 << PD7));
    PORTB &= ~(1 << PB0);
}

static void led1_set(uint8_t on) {
    if (on) PORTD |=  (1 << PD5);
    else    PORTD &= ~(1 << PD5);
}
static void led2_set(uint8_t on) {
    if (on) PORTD |=  (1 << PD6);
    else    PORTD &= ~(1 << PD6);
}
static void led3_set(uint8_t on) {
    if (on) PORTD |=  (1 << PD7);
    else    PORTD &= ~(1 << PD7);
}
static void led4_set(uint8_t on) {
    if (on) PORTB |=  (1 << PB0);
    else    PORTB &= ~(1 << PB0);
}

/* ─────────────────────────────────────────────
   Helpers: float → string (1 decimal place)
───────────────────────────────────────────── */
static void ftoa_1dp(float val, char *buf, uint8_t buflen) {
    /* Avoid using printf %f which pulls in large library code.
       Instead, use a lightweight manual conversion. */
    int32_t integer = (int32_t)val;
    int32_t frac    = (int32_t)((val - (float)integer) * 10.0f);
    if (frac < 0) frac = -frac;
    snprintf(buf, buflen, "%ld.%ld", (long)integer, (long)frac);
}

/* ─────────────────────────────────────────────
   Main
───────────────────────────────────────────── */
int main(void) {
    uart_init();
    spi_init();
    twi_init();
    leds_init();
    all_leds_off();

    /* LCD */
    lcd_init();
    lcd_set_cursor(0, 0);
    lcd_print("LoRa Receiver");

    /* LoRa */
    if (!lora_begin()) {
        lcd_set_cursor(0, 1);
        lcd_print("LoRa Failed");
        while (1);
    }

    lcd_set_cursor(0, 1);
    lcd_print("Waiting...");

    while (1) {
        if (lora_parse_packet()) {
            int16_t rssi     = lora_packet_rssi();
            float   distance = calculate_distance(rssi);

            /* Serial output */
            char dbuf[16];
            ftoa_1dp(distance, dbuf, sizeof(dbuf));
            uart_puts("Distance: ");
            uart_puts(dbuf);
            uart_puts(" m\r\n");

            /* LCD */
            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_print("Distance:");
            lcd_set_cursor(0, 1);

            char lbuf[12];
            ftoa_1dp(distance, lbuf, sizeof(lbuf));
            /* Truncate to fit 16-char display */
            char row2[17];
            snprintf(row2, sizeof(row2), "%s m", lbuf);
            lcd_print(row2);

            /* LED logic (cumulative) */
            all_leds_off();

            if (distance > 60.0f) {        /* VERY FAR — blink LED1 */
                led1_set(1);
                _delay_ms(300);
                led1_set(0);
                _delay_ms(300);
            } else if (distance > 40.0f) { /* FAR */
                led1_set(1);
                led2_set(1);
            } else if (distance > 20.0f) { /* NEAR */
                led1_set(1);
                led2_set(1);
                led3_set(1);
            } else {                       /* VERY NEAR */
                led1_set(1);
                led2_set(1);
                led3_set(1);
                led4_set(1);
            }
        }
    }
}
