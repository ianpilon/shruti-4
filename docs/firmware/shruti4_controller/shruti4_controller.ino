/*
  Shruti 4 · panel controller (Phase 2)

  Reads the physical panel and sends it as USB MIDI, following docs/CONTROL-MAP.md.
  Target: Raspberry Pi Pico (RP2040) with the Arduino core and Adafruit TinyUSB
  ("Tools > USB Stack > Adafruit TinyUSB"), or a Teensy 4.x with "USB Type: MIDI".

  Wiring summary (see docs/HARDWARE.md):
    - Three 74HC4067 16-channel analog muxes on A0, A1, A2. Shared select lines S0..S3.
    - Every pot, fader, ribbon and rotary-switch ladder is a divider 3.3V .. GND, wiper to a mux input.
    - Ribbons get a 10k pull-down on the wiper so "untouched" reads ~0.
    - Rotary voice switches use a 6-resistor ladder, so they read as a 6-step pot.
    - Ten toggles on GPIO with internal pull-ups, closed = LOW.

  Channel plan: boxes 1..4 = MIDI channels 1..4, header = channel 5.
*/

#include <Arduino.h>

#if defined(ARDUINO_ARCH_RP2040)
  #include <Adafruit_TinyUSB.h>
  #include <MIDI.h>
  Adafruit_USBD_MIDI usbMidi;
  MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usbMidi, MIDI);
  #define SEND_CC(cc, val, ch) MIDI.sendControlChange((cc), (val), (ch))
#elif defined(TEENSYDUINO)
  #define SEND_CC(cc, val, ch) usbMIDI.sendControlChange((cc), (val), (ch))
#else
  #error "Target a Raspberry Pi Pico (TinyUSB) or a Teensy 4.x (USB Type: MIDI)."
#endif

// ---------- pins ----------
const uint8_t MUX_SEL[4]  = {2, 3, 4, 5};        // S0..S3, shared by all three muxes
const uint8_t MUX_SIG[3]  = {A0, A1, A2};        // one analog pin per 74HC4067
const uint8_t TOGGLE_PIN[10] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

// ---------- control table ----------
// Each analog control: which mux, which channel, MIDI channel, CC, and kind.
enum Kind : uint8_t { POT, FADER, RIBBON, VOICE_SW };
struct AnalogCtl { uint8_t mux, chan, midiCh, cc; Kind kind; };

// Per box: Pump 23, Drive 24, Grit 25, Room 26, Level 27, Pan 28, Fader 29, Ribbon 30, Voice switch 20.
// Mux 0 = box 1 + box 2, Mux 1 = box 3 + box 4, Mux 2 = header + rotary switch ladders.
const AnalogCtl CTL[] = {
  // box 1 (mux 0, ch 0..7)
  {0, 0, 1, 23, POT}, {0, 1, 1, 24, POT}, {0, 2, 1, 25, POT}, {0, 3, 1, 26, POT}, {0, 4, 1, 27, POT}, {0, 5, 1, 28, POT}, {0, 6, 1, 29, FADER}, {0, 7, 1, 30, RIBBON},
  // box 2 (mux 0, ch 8..15)
  {0, 8, 2, 23, POT}, {0, 9, 2, 24, POT}, {0, 10, 2, 25, POT}, {0, 11, 2, 26, POT}, {0, 12, 2, 27, POT}, {0, 13, 2, 28, POT}, {0, 14, 2, 29, FADER}, {0, 15, 2, 30, RIBBON},
  // box 3 (mux 1, ch 0..7)
  {1, 0, 3, 23, POT}, {1, 1, 3, 24, POT}, {1, 2, 3, 25, POT}, {1, 3, 3, 26, POT}, {1, 4, 3, 27, POT}, {1, 5, 3, 28, POT}, {1, 6, 3, 29, FADER}, {1, 7, 3, 30, RIBBON},
  // box 4 (mux 1, ch 8..15)
  {1, 8, 4, 23, POT}, {1, 9, 4, 24, POT}, {1, 10, 4, 25, POT}, {1, 11, 4, 26, POT}, {1, 12, 4, 27, POT}, {1, 13, 4, 28, POT}, {1, 14, 4, 29, FADER}, {1, 15, 4, 30, RIBBON},
  // header (mux 2, ch 0..3): Age 42, Breath 43, Width 44, Level 45
  {2, 0, 5, 42, POT}, {2, 1, 5, 43, POT}, {2, 2, 5, 44, POT}, {2, 3, 5, 45, POT},
  // voice switches as resistor ladders (mux 2, ch 4..7)
  {2, 4, 1, 20, VOICE_SW}, {2, 5, 2, 20, VOICE_SW}, {2, 6, 3, 20, VOICE_SW}, {2, 7, 4, 20, VOICE_SW},
};
const uint8_t N_CTL = sizeof(CTL) / sizeof(CTL[0]);

// Toggles: index -> MIDI channel and CC. Box off/on 21, box octave 22, header interval 40, header sync 41.
struct ToggleCtl { uint8_t midiCh, cc; };
const ToggleCtl TOG[10] = {
  {1, 21}, {1, 22}, {2, 21}, {2, 22}, {3, 21}, {3, 22}, {4, 21}, {4, 22},   // boxes 1..4: on, octave
  {5, 40}, {5, 41},                                                          // header: fourth/fifth, pump sync
};

// ---------- state ----------
uint16_t smooth[64];       // moving-average state per analog control (12-bit)
int16_t  lastSent[64];     // last MIDI value sent, -1 = never
bool     ribbonDown[64];
uint8_t  togState[10];
uint32_t togChangedAt[10];

const uint16_t RIBBON_TOUCH = 40;      // 12-bit reading above this = finger present (with 10k pull-down)
const uint8_t  RIBBON_RATE_HZ = 100;   // send rate while touched

void selectMux(uint8_t ch) {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(MUX_SEL[i], (ch >> i) & 1);
  delayMicroseconds(5);                // settle
}

uint16_t readCtl(const AnalogCtl& c) {
  selectMux(c.chan);
  analogRead(MUX_SIG[c.mux]);          // throw one away after switching
  return analogRead(MUX_SIG[c.mux]);   // 0..4095 with 12-bit resolution
}

uint8_t to7bit(uint16_t v12) { return v12 >> 5; }

void setup() {
  analogReadResolution(12);
  for (uint8_t i = 0; i < 4; i++) pinMode(MUX_SEL[i], OUTPUT);
  for (uint8_t i = 0; i < 10; i++) { pinMode(TOGGLE_PIN[i], INPUT_PULLUP); togState[i] = 2; }
  for (uint8_t i = 0; i < 64; i++) { smooth[i] = 0; lastSent[i] = -1; ribbonDown[i] = false; }
#if defined(ARDUINO_ARCH_RP2040)
  MIDI.begin(MIDI_CHANNEL_OMNI);
  while (!TinyUSBDevice.mounted()) delay(1);
#endif
}

uint32_t lastRibbonSend = 0;

void loop() {
  const uint32_t now = millis();
  const bool ribbonTick = (now - lastRibbonSend) >= (1000 / RIBBON_RATE_HZ);

  for (uint8_t i = 0; i < N_CTL; i++) {
    const AnalogCtl& c = CTL[i];
    uint16_t raw = readCtl(c);

    if (c.kind == RIBBON) {
      // Momentary: send position while touched, one 0 on release.
      bool down = raw > RIBBON_TOUCH;
      if (down && ribbonTick) { uint8_t v = to7bit(raw); if (v != lastSent[i]) { SEND_CC(c.cc, v, c.midiCh); lastSent[i] = v; } }
      if (!down && ribbonDown[i]) { SEND_CC(c.cc, 0, c.midiCh); lastSent[i] = 0; }
      ribbonDown[i] = down;
      continue;
    }

    // 4-sample moving average, then hysteresis of one 7-bit step.
    smooth[i] = (smooth[i] * 3 + raw) / 4;
    uint8_t v;
    if (c.kind == VOICE_SW) {
      // Six-step ladder: 0..5, sent as 0,25,51,76,102,127 so the receiver can divide by 25.4.
      uint8_t pos = (uint8_t)min(5, (smooth[i] * 6) / 4096);
      v = (uint8_t)(pos * 127 / 5);
    } else {
      v = to7bit(smooth[i]);
    }
    if (lastSent[i] < 0 || abs((int)v - lastSent[i]) >= (c.kind == VOICE_SW ? 1 : 2) || (c.kind != VOICE_SW && (v == 0 || v == 127) && v != lastSent[i])) {
      SEND_CC(c.cc, v, c.midiCh);
      lastSent[i] = v;
    }
  }
  if (ribbonTick) lastRibbonSend = now;

  // Toggles, 20 ms debounce, send once per change. Closed = LOW = on.
  for (uint8_t i = 0; i < 10; i++) {
    uint8_t s = digitalRead(TOGGLE_PIN[i]) == LOW ? 1 : 0;
    if (s != togState[i]) {
      if (togState[i] == 2 || now - togChangedAt[i] > 20) {
        togState[i] = s; togChangedAt[i] = now;
        SEND_CC(TOG[i].cc, s ? 127 : 0, TOG[i].midiCh);
      }
    }
  }

#if defined(ARDUINO_ARCH_RP2040)
  MIDI.read();
#else
  usbMIDI.read();
#endif
  delay(4);   // ~200 Hz scan
}
