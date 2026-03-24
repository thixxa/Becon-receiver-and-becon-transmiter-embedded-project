/*
 * receiver.c — AVR C equivalent of corected_receiver.ino
 *
 * Target MCU : ATmega328P (Arduino Uno/Nano)
 * F_CPU       : 16 MHz
 *
 * Peripherals used:
 *   - UART0        : 9600 baud (Serial debug)
 *   - SPI          : LoRa module (via semtech SX127x driver, see lora.h)
 *   - I2C (TWI)    : PCF8574-backed LCD at address 0x27
 *   - GPIO PD5-PD7 : LED1-LED3
 *   - GPIO PB0     : LED4
 *
 * Third-party C libraries expected (drop-in replacements for Arduino libs):
 *   lora.h / lora.c  — arduino-lora port:  https://github.com/sandeepmistry/arduino-LoRa
 *   i2c_lcd.h        — Peter Fleury's LCD over I2C, or any compatible header
 *   i2c_master.h     — Peter Fleury's i2cmaster library
 *
 * NOTE: Replace the lora_*, lcd_* and i2c_* calls below with whichever
 * AVR C LoRa / I2C-LCD driver you have in your project.  The logic is
 * identical to the original sketch.
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include <math.h>

// bring in AVR C peripheral drivers 
#include "lora.h"                   //SX127x LoRa driver  
#include "i2c_master.h"             //Peter Fleury TWI    
#include "i2c_lcd.h"                // I2C LCD 16x2        

/* ================================================================
 * LED pin definitions
 *   LED1 = PD5  (Arduino D5)
 *   LED2 = PD6  (Arduino D6)
 *   LED3 = PD7  (Arduino D7)
 *   LED4 = PB0  (Arduino D8)
 * ================================================================ */
#define LED1_PORT  PORTD
#define LED1_DDR   DDRD
#define LED1_PIN   PD5

#define LED2_PORT  PORTD
#define LED2_DDR   DDRD
#define LED2_PIN   PD6

#define LED3_PORT  PORTD
#define LED3_DDR   DDRD
#define LED3_PIN   PD7

#define LED4_PORT  PORTB
#define LED4_DDR   DDRB
#define LED4_PIN   PB0

// UART helpers (9600, 8N1)
static void uart_init(void)
{
    // UBRR = F_CPU / (16 * baud) - 1 
    uint16_t ubrr = (uint16_t)(F_CPU / (16UL * 9600UL)) - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr);
    UCSR0B = (1 << TXEN0);                          //TX enable                
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);         //8-bit, 1 stop, no parity 
}

static void uart_putchar(char c)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s) uart_putchar(*s++);
}

//Minimal printf-style float printer: prints val with 'decimals' d.p. 
static void uart_print_float(float val, uint8_t decimals)
{
    char buf[16];
    // Use dtostrf from avr-libc
    extern char *dtostrf(double, signed char, unsigned char, char *);
    dtostrf((double)val, 6, decimals, buf);
    uart_puts(buf);
}


// LED helpers
static void leds_init(void)
{
    LED1_DDR |= (1 << LED1_PIN);
    LED2_DDR |= (1 << LED2_PIN);
    LED3_DDR |= (1 << LED3_PIN);
    LED4_DDR |= (1 << LED4_PIN);
}

static void all_leds_off(void)
{
    LED1_PORT &= ~(1 << LED1_PIN);
    LED2_PORT &= ~(1 << LED2_PIN);
    LED3_PORT &= ~(1 << LED3_PIN);
    LED4_PORT &= ~(1 << LED4_PIN);
}

static inline void led1_on(void) { LED1_PORT |= (1 << LED1_PIN); }
static inline void led2_on(void) { LED2_PORT |= (1 << LED2_PIN); }
static inline void led3_on(void) { LED3_PORT |= (1 << LED3_PIN); }
static inline void led4_on(void) { LED4_PORT |= (1 << LED4_PIN); }

static inline void led1_off(void) { LED1_PORT &= ~(1 << LED1_PIN); }


// Distance model parameters
#define RSSI_0  (-40.0f)                // RSSI at 1 metre      
#define N_EXP     2.7f                  // path-loss exponent   
#define D0        1.0f                  // reference distance m 

static float calculate_distance(int rssi)
{
    return D0 * powf(10.0f, (RSSI_0 - (float)rssi) / (10.0f * N_EXP));
}


// main
int main(void)
{
    //  UART 
    uart_init();

    //  LEDs 
    leds_init();
    all_leds_off();

    // I2C + LCD  
    i2c_init();                          // TWI initialise             
    lcd_init(LCD_DISP_ON);               // 16x2, display on           
    lcd_clrscr();
    lcd_puts("LoRa Receiver");

    // LoRa 
    if (lora_begin(433E6) != 0) {        // 0 = success in AVR driver  
        lcd_gotoxy(0, 1);
        lcd_puts("LoRa Failed");
        while (1);                       // halt if LoRa init fails
    }

    lcd_gotoxy(0, 1);
    lcd_puts("Waiting...");


    // Main loop
    while (1)
    {
        int packet_size = lora_parse_packet(0);

        if (packet_size > 0)
        {
            // Drain incoming bytes (we only need RSSI) 
            while (lora_available())
                lora_read();

            int rssi     = lora_packet_rssi();
            float dist   = calculate_distance(rssi);

            // Serial output
            uart_puts("Distance: ");
            uart_print_float(dist, 2);
            uart_puts(" m\r\n");

            // LCD output
            lcd_clrscr();
            lcd_gotoxy(0, 0);
            lcd_puts("Distance:");
            lcd_gotoxy(0, 1);
            {
                char buf[10];
                extern char *dtostrf(double, signed char, unsigned char, char *);
                dtostrf((double)dist, 5, 1, buf);
                lcd_puts(buf);
            }
            lcd_puts(" m");

            // LED logic (cumulative)
            all_leds_off();

            if (dist > 60.0f)           // VERY FAR — blink LED1 
            {
                led1_on();
                _delay_ms(300);
                led1_off();
                _delay_ms(300);
            }
            else if (dist > 40.0f)      // FAR 
            {
                led1_on();
                led2_on();
            }
            else if (dist > 20.0f)      // NEAR
            {
                led1_on();
                led2_on();
                led3_on();
            }
            else                        // VERY NEAR
            {
                led1_on();
                led2_on();
                led3_on();
                led4_on();
            }
        }
    }

    return 0; // never reached
}
