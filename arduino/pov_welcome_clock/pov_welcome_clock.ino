/*
 * Propeller (POV) LED Display - "WELCOME  HH:MM:SS"
 * Board : Arduino UNO (ATmega328P)          Language: Arduino C++
 *
 * Hardware
 *   8 LEDs, each with a 330 ohm resistor : D3..D10  (bit0 = D3 ... bit7 = D10)
 *   A3144 Hall sensor OUT                : D2 (INT0)  + magnet fixed on the base
 *   Optional DS3231 RTC (I2C)            : SDA = A4, SCL = A5   (see USE_DS3231)
 *
 */

#include <avr/pgmspace.h>
#include <string.h>
#include <stdio.h>

// ----------------------------- USER SETTINGS -----------------------------
#define USE_DS3231   0                 // 0 = software clock, 1 = DS3231 RTC module
const char    GREETING[]     = "WELCOME";
const uint8_t COLS_PER_REV   = 120;    // columns per rotation (3 degrees each)
const bool    REVERSE_TEXT   = false;  // true if text looks mirrored
const bool    FLIP_ROWS      = false;  // true if text looks upside down
const uint8_t TIME_ADJUST_S  = 10;     // seconds added to compile time (upload delay)
// -------------------------------------------------------------------------

#if USE_DS3231
  #include <Wire.h>
  const uint8_t RTC_ADDR = 0x68;
#endif

const uint8_t HALL_PIN = 2;            // INT0

// ---------- 5x7 font: 5 columns per character, bit0 = top dot ----------
const char FONT_CHARS[] = " 0123456789:CELMOW";
const uint8_t FONT[][5] PROGMEM = {
  {0x00, 0x00, 0x00, 0x00, 0x00},  // space
  {0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0
  {0x00, 0x42, 0x7F, 0x40, 0x00},  // 1
  {0x42, 0x61, 0x51, 0x49, 0x46},  // 2
  {0x21, 0x41, 0x45, 0x4B, 0x31},  // 3
  {0x18, 0x14, 0x12, 0x7F, 0x10},  // 4
  {0x27, 0x45, 0x45, 0x45, 0x39},  // 5
  {0x3C, 0x4A, 0x49, 0x49, 0x30},  // 6
  {0x01, 0x71, 0x09, 0x05, 0x03},  // 7
  {0x36, 0x49, 0x49, 0x49, 0x36},  // 8
  {0x06, 0x49, 0x49, 0x29, 0x1E},  // 9
  {0x00, 0x14, 0x14, 0x00, 0x00},  // :
  {0x3E, 0x41, 0x41, 0x41, 0x22},  // C
  {0x7F, 0x49, 0x49, 0x49, 0x41},  // E
  {0x7F, 0x40, 0x40, 0x40, 0x40},  // L
  {0x7F, 0x02, 0x0C, 0x02, 0x7F},  // M
  {0x3E, 0x41, 0x41, 0x41, 0x3E},  // O
  {0x3F, 0x40, 0x38, 0x40, 0x3F}   // W
};

// ---------- shared with the interrupt -> volatile ----------
volatile uint32_t lastHallTime = 0;
volatile uint32_t revTime      = 0;    // microseconds per revolution
volatile bool     newRev       = false;

// ---------- clock ----------
uint8_t  hours = 0, minutes = 0, seconds = 0;
uint32_t lastSecondMs = 0;
char     displayText[17];              // "WELCOME 12:34:56" + '\0'
int16_t  builtSecond = -1;             // second that displayText was built for

// ===================== Hall sensor interrupt =====================
void hallISR() {
  uint32_t now = micros();
  uint32_t dt  = now - lastHallTime;
  if (dt < 5000UL) return;             // noise filter
  lastHallTime = now;
  revTime = dt;
  newRev  = true;
}

// ===================== Clock functions =====================
void tickSecond() {
  if (++seconds >= 60) {
    seconds = 0;
    if (++minutes >= 60) {
      minutes = 0;
      if (++hours >= 24) hours = 0;
    }
  }
}

// __TIME__ is text like "14:05:09", filled in by the compiler
void initClockFromCompileTime() {
  hours   = (__TIME__[0] - '0') * 10 + (__TIME__[1] - '0');
  minutes = (__TIME__[3] - '0') * 10 + (__TIME__[4] - '0');
  seconds = (__TIME__[6] - '0') * 10 + (__TIME__[7] - '0');
  for (uint8_t i = 0; i < TIME_ADJUST_S; i++) tickSecond();
  lastSecondMs = millis();
}

#if USE_DS3231
uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

void readRTC() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write((uint8_t)0x00);           // start at seconds register
  Wire.endTransmission();
  Wire.requestFrom(RTC_ADDR, (uint8_t)3);
  if (Wire.available() >= 3) {
    seconds = bcd2dec(Wire.read() & 0x7F);
    minutes = bcd2dec(Wire.read() & 0x7F);
    hours   = bcd2dec(Wire.read() & 0x3F);   // 24-hour mode
  }
}
#endif

void updateClock() {
#if USE_DS3231
  static uint32_t lastRead = 0;
  if (millis() - lastRead >= 250) { lastRead = millis(); readRTC(); }
#else
  while (millis() - lastSecondMs >= 1000UL) {
    lastSecondMs += 1000UL;
    tickSecond();
  }
#endif
}

void buildDisplayText() {
  snprintf(displayText, sizeof(displayText), "%s %02u:%02u:%02u",
           GREETING, (unsigned)hours, (unsigned)minutes, (unsigned)seconds);
  builtSecond = seconds;
}

// ===================== LED + font functions =====================
// LEDs: bit0..bit4 -> PD3..PD7, bit5..bit7 -> PB0..PB2 (same as D3..D10)
inline void showColumn(uint8_t p) {
  if (FLIP_ROWS) {
    uint8_t r = 0;
    for (uint8_t i = 0; i < 8; i++) if (p & (1 << i)) r |= (1 << (7 - i));
    p = r;
  }
  PORTD = (PORTD & 0x07) | ((p & 0x1F) << 3);   // keep PD0..PD2 (serial + Hall)
  PORTB = (PORTB & ~0x07) | (p >> 5);
}

// column c of the whole text (5 dots + 1 blank gap per character)
uint8_t getColumn(uint8_t c) {
  uint8_t idx = c / 6, k = c % 6;
  if (k == 5) return 0;
  char ch = displayText[idx];
  for (uint8_t i = 0; FONT_CHARS[i] != '\0'; i++)
    if (FONT_CHARS[i] == ch) return pgm_read_byte(&FONT[i][k]);
  return 0;
}

// ===================== Arduino entry points =====================
void setup() {
  DDRD |= 0xF8;                        // PD3..PD7 outputs
  DDRB |= 0x07;                        // PB0..PB2 outputs
  pinMode(HALL_PIN, INPUT_PULLUP);     // A3144 is open-collector
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hallISR, FALLING);

#if USE_DS3231
  Wire.begin();
  readRTC();
#else
  initClockFromCompileTime();
#endif
  buildDisplayText();
}

void loop() {
  updateClock();
  if (!newRev) return;                 // wait for a new revolution

  noInterrupts();
  uint32_t T = revTime;
  newRev = false;
  interrupts();
  if (T > 200000UL) return;            // too slow / first reading

  if (seconds != builtSecond) buildDisplayText();

  uint8_t  total = strlen(displayText) * 6;
  if (total > COLS_PER_REV) total = COLS_PER_REV;
  uint32_t colTime = T / COLS_PER_REV;
  uint32_t start   = micros();

  for (uint8_t c = 0; c < COLS_PER_REV; c++) {
    uint8_t pattern = 0;
    if (c < total) pattern = getColumn(REVERSE_TEXT ? (total - 1 - c) : c);
    showColumn(pattern);
    while ((uint32_t)(micros() - start) < (uint32_t)(c + 1) * colTime) { }
  }
  showColumn(0);
}
