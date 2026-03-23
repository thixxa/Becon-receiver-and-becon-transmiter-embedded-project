/*
 * transmitter.c — AVR C equivalent of corected_transmiter.ino
 *
 * Target MCU : ATmega328P (Arduino Uno/Nano)
 * F_CPU       : 16 MHz
 *
 * This file implements the IR-sensor line-follower logic originally in the
 * Arduino sketch.  The original file contained two overlapping loop() blocks;
 * the second (more complete) version — which includes Serial debug prints —
 * is used here as the authoritative source.
 *
 * Pin mapping (adjust to your wiring):
 *   IR_OUT_LEFT  = PC0  (Arduino A0)
 *   IR_LEFT      = PC1  (Arduino A1)
 *   IR_MID       = PC2  (Arduino A2)
 *   IR_RIGHT     = PC3  (Arduino A3)
 *   IR_OUT_RIGHT = PC4  (Arduino A4)
 *
 * Motor driver  (L298N / L293D, example):
 *   Left motor  forward  = PD5 (OC0B PWM) and PD4 direction
 *   Right motor forward  = PD6 (OC0A PWM) and PD7 direction
 *   (Change motor_forward() / stop_motors() to match your driver.)
 *
 * Speed constants — tune to your robot:
 *   baseSpeed  = 180  (0-255 PWM)
 *   turnSpeed  = 120
 *   hardTurn   = 80
 *   searchSpeed= 130
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>

/* ================================================================
 * UART helpers (9600 8N1)
 * ================================================================ */
static void uart_init(void)
{
    uint16_t ubrr = (uint16_t)(F_CPU / (16UL * 9600UL)) - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
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

static void uart_println(const char *s)
{
    uart_puts(s);
    uart_puts("\r\n");
}

/* Print "KEY: VALUE " */
static void uart_print_int_label(const char *label, int val)
{
    char buf[8];
    uart_puts(label);
    itoa(val, buf, 10);
    uart_puts(buf);
    uart_putchar(' ');
}

/* ================================================================
 * IR sensor pin definitions  (all on PORTC / ADC pins)
 * ================================================================ */
#define IR_DDR   DDRC
#define IR_PIN   PINC

#define IR_OUT_LEFT_BIT  PC0
#define IR_LEFT_BIT      PC1
#define IR_MID_BIT       PC2
#define IR_RIGHT_BIT     PC3
#define IR_OUT_RIGHT_BIT PC4

static void ir_sensors_init(void)
{
    /* Set all IR pins as input, no pull-up (sensors supply their own logic) */
    IR_DDR &= ~(
        (1 << IR_OUT_LEFT_BIT)  |
        (1 << IR_LEFT_BIT)      |
        (1 << IR_MID_BIT)       |
        (1 << IR_RIGHT_BIT)     |
        (1 << IR_OUT_RIGHT_BIT)
    );
}

static inline int ir_read(uint8_t bit)
{
    return (IR_PIN & (1 << bit)) ? 1 : 0;
}

/* ================================================================
 * Motor driver (PWM on OC0A / OC0B — Timer0 fast PWM)
 *
 *   Left  motor  : IN1=PD4, EN1/PWM=PD5(OC0B)
 *   Right motor  : IN2=PD7, EN2/PWM=PD6(OC0A)
 *
 * "forward(leftSpeed, rightSpeed)" — both values 0-255.
 * ================================================================ */
#define MTR_DDR    DDRD
#define MTR_PORT   PORTD

#define L_DIR_PIN  PD4
#define L_PWM_PIN  PD5   /* OC0B */
#define R_DIR_PIN  PD7
#define R_PWM_PIN  PD6   /* OC0A */

static void motors_init(void)
{
    MTR_DDR |= (1 << L_DIR_PIN) | (1 << L_PWM_PIN) |
               (1 << R_DIR_PIN) | (1 << R_PWM_PIN);

    /* Timer0: Fast PWM, non-inverting on OC0A & OC0B, prescaler 64 */
    TCCR0A = (1 << COM0A1) | (1 << COM0B1) |
             (1 << WGM01)  | (1 << WGM00);
    TCCR0B = (1 << CS01)   | (1 << CS00);  /* clk/64 → ~976 Hz PWM */

    OCR0A = 0;
    OCR0B = 0;
}

static void forward(uint8_t left_speed, uint8_t right_speed)
{
    /* Direction pins HIGH = forward for both motors */
    MTR_PORT |=  (1 << L_DIR_PIN) | (1 << R_DIR_PIN);
    OCR0B = left_speed;
    OCR0A = right_speed;
}

static void stop_motors(void)
{
    OCR0A = 0;
    OCR0B = 0;
}

/* ================================================================
 * Speed constants — tune as needed
 * ================================================================ */
#define BASE_SPEED    180
#define TURN_SPEED    120
#define HARD_TURN      80
#define SEARCH_SPEED  130

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    uart_init();
    ir_sensors_init();
    motors_init();

    int last_direction = 0;  /* -1 = last turned left, 1 = right, 0 = straight */

    while (1)
    {
        /* ===== READ SENSOR VALUES ===== */
        int OL = ir_read(IR_OUT_LEFT_BIT);
        int L  = ir_read(IR_LEFT_BIT);
        int M  = ir_read(IR_MID_BIT);
        int R  = ir_read(IR_RIGHT_BIT);
        int OR = ir_read(IR_OUT_RIGHT_BIT);

        /* ===== PRINT SENSOR VALUES ===== */
        uart_print_int_label("OL: ", OL);
        uart_print_int_label(" L: ", L);
        uart_print_int_label(" M: ", M);
        uart_print_int_label(" R: ", R);
        uart_print_int_label(" OR:", OR);
        uart_puts("\r\n");

        /* ===== LINE FOLLOW LOGIC ===== */
        if (L == 0 && M == 1 && R == 0)
        {
            uart_println("Forward");
            forward(BASE_SPEED, BASE_SPEED);
            last_direction = 0;
        }
        else if (L == 1 && M == 0 && R == 0)
        {
            uart_println("Turn Left");
            forward(TURN_SPEED, BASE_SPEED);
            last_direction = -1;
        }
        else if (L == 0 && M == 0 && R == 1)
        {
            uart_println("Turn Right");
            forward(BASE_SPEED, TURN_SPEED);
            last_direction = 1;
        }
        else if (L == 1 && M == 1 && R == 0)
        {
            uart_println("Hard Left");
            forward(HARD_TURN, BASE_SPEED);
            last_direction = -1;
        }
        else if (L == 0 && M == 1 && R == 1)
        {
            uart_println("Hard Right");
            forward(BASE_SPEED, HARD_TURN);
            last_direction = 1;
        }
        else if ((L == 1 && M == 0 && R == 1) || (L == 1 && M == 1 && R == 1))
        {
            uart_println("Straight (All/Center case)");
            forward(BASE_SPEED, BASE_SPEED);
        }
        else if (L == 0 && M == 0 && R == 0)
        {
            uart_println("Searching...");
            if (OL == 1)
            {
                uart_println("Search Left");
                forward(SEARCH_SPEED, BASE_SPEED);
            }
            else if (OR == 1)
            {
                uart_println("Search Right");
                forward(BASE_SPEED, SEARCH_SPEED);
            }
            else
            {
                uart_println("Search Last Direction");
                if (last_direction == -1)
                    forward(SEARCH_SPEED, BASE_SPEED);
                else
                    forward(BASE_SPEED, SEARCH_SPEED);
            }
        }
        else
        {
            uart_println("Stop");
            stop_motors();
        }

        /* ===== RUN FOR 1 SECOND ===== */
        uart_println("Running...");
        _delay_ms(1000);

        /* ===== STOP FOR 0.5 SECOND ===== */
        uart_println("Stopped");
        stop_motors();
        _delay_ms(500);

        uart_println("----------------------");
    }

    return 0; /* never reached */
}
