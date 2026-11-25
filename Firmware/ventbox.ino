/*
  Ventbox - humidity-extraction module (protrudes 8 mm) for the original 1.9 L container lid - firmware v1.2
  (v1.2 = English release; behaviour identical to v1.1)
  Hardware: electronics/kicad/ventbox.kicad_sch rev 1.2
    D1 = button SW1 (to GND)        D2 = FAN_PWM -> R1 -> Q1 AO3400A
    D7 / D8 / D6 = RGB LED D2 (XL-1615RGBC-RF, common anode tied to VBAT) via R3 330 / R4 150 / R5 150
  Licence: CERN-OHL-S v2, see the LICENSE file in the repo root
  Board  : Seeed XIAO nRF52840 (Arduino IDE, "Seeed nRF52 Boards" package)
  Library: Adafruit SHT4x
  Team   : Group 2

  Two functions run INDEPENDENTLY and only share the microcontroller, battery and button:

  1) HUMIDITY EXTRACTION (as in v1.0)
     - Every CHECK_S seconds, measure the humidity in the extraction chamber.
     - RH >= RH_ON    -> run the fan for RUN_S seconds, repeat until RH <= RH_OFF.
     - Short press    -> run a "quick purge" for BURST_S seconds; short press again to stop.
     - Battery < VBAT_MIN -> fan does not run, LED flashes blue / red alternately 3 times.

  2) FRESHNESS TIMER (new in v1.1)
     - Hold the button 2 s (release when the LED flashes green) -> start / reset the 7-day timer.
     - Hold the button 5 s (release when the LED flashes red)   -> switch the timer off.
     - The LED gives a short flash every BLINK_S seconds according to the storage time:
         0 .. 2 days : green              (fresh)
         2 .. 5 days : orange             (use soon)
         5 .. 7 days : red                (stored for a long time)
         > 7 days    : red double flash   (expired)
     - A short press also shows the current timer colour immediately (in addition to the quick purge).
     - Time is counted with the nRF52840 32.768 kHz crystal clock; on power loss (flat battery, firmware
       re-flash) the timer is cleared and the LED stays dark until it is set again.
*/
// Third-party libraries (not part of this repo, see firmware/README.md "Third-party libraries and origin"):
//   Wire.h          – I2C library of the Seeed nRF52 core, Arduino LLC / S. Mistry / Adafruit, LGPL-2.1
//                     https://github.com/Seeed-Studio/Adafruit_nRF52_Arduino
//   Adafruit_SHT4x  – Adafruit Industries, BSD licence (needs Adafruit BusIO, MIT, and Adafruit Unified Sensor, Apache-2.0)
//                     https://github.com/adafruit/Adafruit_SHT4x
#include <Wire.h>
#include "Adafruit_SHT4x.h"

Adafruit_SHT4x sht;

// ---------- pins ----------
const int PIN_FAN = D2;          // AO3400 gate (100 Ω series, 10 kΩ pull-down to GND)
const int PIN_BTN = D1;          // button to GND
const int PIN_LR = D7, PIN_LG = D8, PIN_LB = D6;   // RGB LED cathodes via resistors: LOW = on

// ---------- humidity extraction ----------
const float RH_ON = 85.0, RH_OFF = 75.0;
const uint16_t CHECK_S = 60, RUN_S = 30, BURST_S = 120;
const float VBAT_MIN = 3.35;
// The Sunon UB5U3-700 fan needs a constant supply voltage (datasheet), so no PWM: on/off only.

// ---------- freshness timer ----------
const uint32_t HOUR_MS = 3600000UL;
const uint16_t GREEN_H = 48, ORANGE_H = 72, RED_H = 48;     // 2 + 3 + 2 days = 7 days
const uint16_t BLINK_S = 4;                                   // status flash period
const uint16_t FLASH_MS = 40;                                 // length of each flash
const uint32_t SHORT_MAX_MS = 800, HOLD_SET_MS = 2000, HOLD_OFF_MS = 5000;
// Brightness of each colour (% on-time within one 1 kHz software-PWM period). Green is ~4x brighter than red.
struct Col { uint8_t r, g, b; };
const Col C_GREEN = {0, 60, 0}, C_ORANGE = {100, 14, 0}, C_RED = {100, 0, 0}, C_BLUE = {0, 0, 100};

const uint32_t TICK_MS = 50;

// ---------- state ----------
bool humid = false;
bool fanOn = false;   uint32_t fanStopAt = 0;
uint32_t nextCheck = 0, nextBlink = 0, lastNow = 0;
bool timerOn = false; uint64_t timerMs = 0;                   // 64 bit: does not overflow like millis() after 49 days
bool btnDown = false; uint32_t btnT0 = 0; uint8_t holdStage = 0;

// ---------- LED ----------
void ledOff() { digitalWrite(PIN_LR, HIGH); digitalWrite(PIN_LG, HIGH); digitalWrite(PIN_LB, HIGH); }

void flash(Col c, uint16_t ms) {          // 1 kHz software PWM to mix orange
  for (uint16_t t = 0; t < ms; t++) {
    uint32_t t0 = micros();
    digitalWrite(PIN_LR, c.r ? LOW : HIGH); digitalWrite(PIN_LG, c.g ? LOW : HIGH); digitalWrite(PIN_LB, c.b ? LOW : HIGH);
    while (micros() - t0 < 1000) {
      uint32_t e = (micros() - t0) / 10;  // 0..99
      if (e >= c.r) digitalWrite(PIN_LR, HIGH);
      if (e >= c.g) digitalWrite(PIN_LG, HIGH);
      if (e >= c.b) digitalWrite(PIN_LB, HIGH);
    }
  }
  ledOff();
}
void flashN(Col c, uint8_t n, uint16_t on = 120, uint16_t off = 150) { for (uint8_t i = 0; i < n; i++) { flash(c, on); delay(off); } }

// ---------- timer ----------
uint8_t timerStage() {                    // 0 green, 1 orange, 2 red, 3 expired
  uint32_t h = (uint32_t)(timerMs / HOUR_MS);
  if (h < GREEN_H) return 0;
  if (h < GREEN_H + ORANGE_H) return 1;
  if (h < GREEN_H + ORANGE_H + RED_H) return 2;
  return 3;
}
void showTimer() {
  if (!timerOn) return;
  switch (timerStage()) {
    case 0: flash(C_GREEN, FLASH_MS); break;
    case 1: flash(C_ORANGE, FLASH_MS); break;
    case 2: flash(C_RED, FLASH_MS); break;
    default: flash(C_RED, FLASH_MS); delay(120); flash(C_RED, FLASH_MS); break;   // double flash = expired
  }
}
void timerStart() { timerOn = true; timerMs = 0; flashN(C_GREEN, 3); }
void timerStop()  { timerOn = false; flashN(C_BLUE, 2); }

// ---------- fan ----------
float readVbat() {               // XIAO nRF52840: on-board divider on P0.31, P0.14 (VBAT_ENABLE) always held LOW
  analogReference(AR_INTERNAL_3_0);   // internal 3.0 V reference
  analogReadResolution(12);
  float v = analogRead(PIN_VBAT) * 3.0f / 4095.0f * 1510.0f / 510.0f;   // 1 MΩ / 510 kΩ divider
  return v;                      // DO NOT set VBAT_ENABLE = HIGH (Seeed warns P0.31 may be damaged while charging)
}
void fanStart(uint32_t now, uint16_t sec) {
  if (readVbat() < VBAT_MIN) {
    for (uint8_t i = 0; i < 3; i++) { flash(C_BLUE, 80); delay(80); flash(C_RED, 80); delay(80); }
    return;
  }
  digitalWrite(PIN_FAN, HIGH); fanOn = true; fanStopAt = now + sec * 1000UL;
}
void fanStop() { digitalWrite(PIN_FAN, LOW); fanOn = false; }

// ---------- button ----------
void onShortPress(uint32_t now) {
  if (fanOn) fanStop();
  else { flash(C_BLUE, 60); fanStart(now, BURST_S); }
  delay(150); showTimer();                // quick look at the timer status
}
void handleButton(uint32_t now) {
  bool p = digitalRead(PIN_BTN) == LOW;
  if (p && !btnDown) { btnDown = true; btnT0 = now; holdStage = 0; }
  else if (p && btnDown) {
    uint32_t d = now - btnT0;
    if (holdStage == 0 && d >= HOLD_SET_MS) { holdStage = 1; flash(C_GREEN, 150); }   // release now = set 7 days
    if (holdStage == 1 && d >= HOLD_OFF_MS) { holdStage = 2; flash(C_RED, 150); }     // release now = timer off
  } else if (!p && btnDown) {
    btnDown = false;
    uint32_t d = now - btnT0;
    if (holdStage == 0) { if (d >= 40 && d < SHORT_MAX_MS) onShortPress(now); }
    else if (holdStage == 1) timerStart();
    else timerStop();
  }
}

void setup() {
  pinMode(PIN_FAN, OUTPUT); digitalWrite(PIN_FAN, LOW);
  pinMode(PIN_LR, OUTPUT); pinMode(PIN_LG, OUTPUT); pinMode(PIN_LB, OUTPUT); ledOff();
  pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT); pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, HIGH); digitalWrite(LED_BLUE, HIGH);   // on-board XIAO LED off (hidden)
  pinMode(VBAT_ENABLE, OUTPUT); digitalWrite(VBAT_ENABLE, LOW);   // held LOW for the whole run time
  pinMode(PIN_CHARGING_CURRENT, INPUT);   // P0.13 floating = 50 mA charge; OUTPUT LOW = 100 mA
  pinMode(PIN_BTN, INPUT_PULLUP);
  sht.begin();
  sht.setPrecision(SHT4X_LOW_PRECISION);
  sht.setHeater(SHT4X_NO_HEATER);
  flash(C_RED, 80); flash(C_GREEN, 80); flash(C_BLUE, 80);        // LED self-test at start-up
  lastNow = millis(); nextCheck = lastNow + 2000; nextBlink = lastNow + BLINK_S * 1000UL;
}

void loop() {
  uint32_t now = millis();
  if (timerOn) timerMs += (uint32_t)(now - lastNow);
  lastNow = now;

  handleButton(now);

  // --- humidity extraction ---
  if (fanOn && (int32_t)(now - fanStopAt) >= 0) fanStop();
  if ((int32_t)(now - nextCheck) >= 0) {
    nextCheck = now + CHECK_S * 1000UL;
    sensors_event_t rh, t;
    if (sht.getEvent(&rh, &t)) {
      float h = rh.relative_humidity;
      if (h >= RH_ON) humid = true;
      if (h <= RH_OFF) humid = false;
      if (humid && !fanOn) fanStart(now, RUN_S);
    }
  }

  // --- timer: status flash ---
  if ((int32_t)(now - nextBlink) >= 0) {
    nextBlink = now + BLINK_S * 1000UL;
    if (!btnDown) showTimer();
  }

  delay(TICK_MS);                // the nRF52 core enters low-power mode inside delay()
}
