/*
 * Propeller (POV) LED Display - "WELCOME  HH:MM:SS"
 * MCU : ATmega328P (Arduino UNO)      Language: Embedded C, register level
 * No Arduino library is used.
 *
 * Pin map
 *   LED bit0..bit4 -> PD3..PD7      LED bit5..bit7 -> PB0..PB2
 *   Hall sensor OUT -> PD2 (INT0), internal pull-up
 *
 * Peripherals used
 *   INT0   : falling edge from the Hall sensor  -> marks the start of a revolution
 *   Timer1 : free running, 4 us per tick        -> measures revolution + column timing
 *   Timer2 : CTC mode, 1 ms interrupt           -> software real-time clock (HH:MM:SS)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdint.h>

#define COLS_PER_REV   120          /* columns per rotation (3 degrees each) */
#define REVERSE_TEXT   0            /* 1 = mirror the text                    */
#define TIME_ADJUST_S  10           /* seconds added to compile time          */

/* ---------------- 5x7 font (bit0 = top dot) ---------------- */
static const char FONT_CHARS[] = " 0123456789:CELMOW";
static const uint8_t FONT[][5] PROGMEM = {
    {0x00, 0x00, 0x00, 0x00, 0x00},  /* space */
    {0x3E, 0x51, 0x49, 0x45, 0x3E},  /* 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00},  /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46},  /* 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31},  /* 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10},  /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39},  /* 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30},  /* 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03},  /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36},  /* 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E},  /* 9 */
    {0x00, 0x14, 0x14, 0x00, 0x00},  /* : */
    {0x3E, 0x41, 0x41, 0x41, 0x22},  /* C */
    {0x7F, 0x49, 0x49, 0x49, 0x41},  /* E */
    {0x7F, 0x40, 0x40, 0x40, 0x40},  /* L */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},  /* M */
    {0x3E, 0x41, 0x41, 0x41, 0x3E},  /* O */
    {0x3F, 0x40, 0x38, 0x40, 0x3F}   /* W */
};

/* ---------------- shared variables (changed inside ISRs) ---------------- */
static volatile uint16_t lastCapture = 0;
static volatile uint16_t revTicks    = 0;     /* ticks(4 us) per revolution */
static volatile uint8_t  newRev      = 0;

static volatile uint16_t ms = 0;              /* software clock */
static volatile uint8_t  hh = 0, mm = 0, ss = 0;

static char text[17];                         /* "WELCOME 12:34:56" */

/* ---------------- Interrupt: Hall sensor ---------------- */
ISR(INT0_vect)
{
    uint16_t now = TCNT1;
    uint16_t dt  = now - lastCapture;         /* unsigned math handles overflow */
    if (dt < 1250) return;                    /* < 5 ms -> noise */
    lastCapture = now;
    revTicks    = dt;
    newRev      = 1;
}

/* ---------------- Interrupt: Timer2 every 1 ms ---------------- */
ISR(TIMER2_COMPA_vect)
{
    if (++ms >= 1000) {
        ms = 0;
        if (++ss >= 60) {
            ss = 0;
            if (++mm >= 60) {
                mm = 0;
                if (++hh >= 24) hh = 0;
            }
        }
    }
}

/* ---------------- helpers ---------------- */
static inline void showColumn(uint8_t v)
{
    PORTD = (PORTD & 0x07) | ((v & 0x1F) << 3);
    PORTB = (PORTB & ~0x07) | (v >> 5);
}

static uint8_t getColumn(uint8_t c)
{
    uint8_t idx = c / 6, k = c % 6;
    if (k == 5) return 0;
    char ch = text[idx];
    for (uint8_t i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == ch) return pgm_read_byte(&FONT[i][k]);
    return 0;
}

static void buildText(uint8_t h, uint8_t m, uint8_t s)
{
    memcpy(text, "WELCOME ", 8);
    text[8]  = '0' + h / 10;  text[9]  = '0' + h % 10;  text[10] = ':';
    text[11] = '0' + m / 10;  text[12] = '0' + m % 10;  text[13] = ':';
    text[14] = '0' + s / 10;  text[15] = '0' + s % 10;  text[16] = '\0';
}

int main(void)
{
    /* GPIO */
    DDRD  |= 0xF8;  DDRB |= 0x07;             /* LED pins = outputs */
    DDRD  &= ~(1 << PD2);
    PORTD |=  (1 << PD2);                     /* pull-up for Hall sensor */

    /* Timer1: free running, prescaler 64 -> 4 us per tick */
    TCCR1A = 0;
    TCCR1B = (1 << CS11) | (1 << CS10);

    /* Timer2: CTC, prescaler 128 -> 125 kHz, compare 124 -> 1 kHz (1 ms) */
    TCCR2A = (1 << WGM21);
    TCCR2B = (1 << CS22) | (1 << CS20);
    OCR2A  = 124;
    TIMSK2 = (1 << OCIE2A);

    /* INT0 on falling edge */
    EICRA = (1 << ISC01);
    EIMSK = (1 << INT0);

    /* set clock from compile time ("HH:MM:SS") */
    hh = (__TIME__[0] - '0') * 10 + (__TIME__[1] - '0');
    mm = (__TIME__[3] - '0') * 10 + (__TIME__[4] - '0');
    ss = (__TIME__[6] - '0') * 10 + (__TIME__[7] - '0');
    for (uint8_t i = 0; i < TIME_ADJUST_S; i++) {
        if (++ss >= 60) { ss = 0; if (++mm >= 60) { mm = 0; if (++hh >= 24) hh = 0; } }
    }

    sei();

    for (;;) {
        while (!newRev) { }                   /* wait for start of a revolution */

        cli();
        uint16_t T = revTicks;
        uint8_t  h = hh, m = mm, s = ss;      /* consistent snapshot of the time */
        newRev = 0;
        sei();

        buildText(h, m, s);
        const uint8_t total = 16 * 6;         /* 16 characters x 6 columns */
        uint16_t colTicks = T / COLS_PER_REV;
        uint16_t next = TCNT1;

        for (uint8_t c = 0; c < COLS_PER_REV; c++) {
            uint8_t pattern = 0;
            if (c < total)
                pattern = getColumn(REVERSE_TEXT ? (total - 1 - c) : c);
            showColumn(pattern);

            next += colTicks;
            while ((int16_t)(TCNT1 - next) < 0) { }
        }
        showColumn(0);
    }
}
