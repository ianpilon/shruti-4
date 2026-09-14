# Shruti 4 · Hardware build guide

This is the plan for turning `shruti-panel.html` into a physical instrument. The web page is the reference design: every control on it is a real part, nothing lights up, and there are no presets. The panel is the sound.

Companion documents:

- `CONTROL-MAP.md` – every control, its range, its curve, and its MIDI CC number.
- `ENGINE.md` – the sound engine, written so it can be ported line for line.
- `firmware/shruti4_controller/shruti4_controller.ino` – a starter sketch that turns the panel into a USB MIDI controller.

## 1. Architecture: three layers, built in three phases

The design is split so the panel and the sound never have to know about each other.

```
 control surface  ──►  parameter table  ──►  sound engine
 (knobs, faders,       (one number per        (oscillators, bellows,
  switches, ribbons)    control, 0..1)         drive, reverb)
```

- **Phase 1, done.** Everything in software on a tablet or laptop. That is the web page.
- **Phase 2, hybrid.** The real panel, wired to a small microcontroller that speaks USB MIDI. The web page listens over Web MIDI and plays the sound. You get the physical feel and can rework the panel before committing to a case. The engine is unchanged.
- **Phase 3, standalone.** The same panel with the engine running inside the box. Two routes, see section 6.

Build Phase 2 first. Almost every decision about the panel gets made there, cheaply.

## 2. The panel

Four identical box sections side by side, a header strip above them. Everything the web page shows, in the same positions.

### Per box (×4)

| Control | Part | Notes |
|---|---|---|
| Voice | 6-position rotary switch, 1 pole | Reed · Bed · Deep · Haze · Pulse · Glow, printed around it. Top-centre of the box. |
| Off / On | SPST toggle | Top-left corner. Position is the only indicator. |
| 1 OCT ↓ | SPST toggle | Top-right corner. |
| Pump, Drive, Grit | 3 × 10 kΩ linear pot | Top row of knobs. |
| Room, Level, Pan | 3 × 10 kΩ linear pot | Bottom row of knobs. Pan has a centre detent if you can get one. |
| Voice fader | 100 mm slide pot, 10 kΩ linear | The "Bright · Full / Dark · Soft" fader. Its position is the setting. |
| Squeeze ribbon | 100 mm membrane position sensor (SoftPot type), wide format if available | Momentary. Reads nothing at rest. Mounted beside the fader, same length, centred on it. |

### Header (×1)

| Control | Part | Notes |
|---|---|---|
| Fourth / Fifth | SPDT or SPST toggle | Chooses the interval for all four boxes. |
| Pump Sync | SPST toggle | Box 1 becomes the clock; the other Pump knobs read as fractions of it. |
| Age, Breath, Width, Level | 4 × 10 kΩ linear pot | Right-aligned, master knobs. Level is the master volume. |

### Totals

| Part | Qty |
|---|---|
| 10 kΩ linear rotary pot | 28 |
| 100 mm slide pot, 10 kΩ linear | 4 |
| 100 mm membrane ribbon | 4 |
| 6-position rotary switch | 4 |
| SPST toggle | 10 |
| Knob caps for pots | 28 |
| Fader caps | 4 |

Suggested part families, all standard and cheap: Alpha or Bourns 9 mm or 16 mm rotary pots; Alps RS60 or Bourns PTA series 100 mm slide pots; Spectra Symbol SoftPot 100 mm (or the 200 mm cut to length is not possible, buy the right length); Lorlin CK or Alpha SR series rotary switches, set the stop to 6 positions; any panel-mount SPST toggle. Buy pots with the same shaft type so one set of caps fits all.

### Dimensions

A comfortable panel is about 480 mm wide by 300 mm deep. Each box column is about 110 mm wide. The header strip is 45 mm tall. The fader and ribbon rows are 130 mm tall, which gives the 100 mm travel plus labels. The knob block needs about 110 mm. Print the legends on the panel exactly as the web page shows them, including the fader scale marks at 3, 5, 7 and the Push / Rest ends of the ribbon.

## 3. Reading the panel: the electronics

Everything on the panel is either a voltage divider (pots, faders, ribbons) or a switch. A single microcontroller reads all of it.

### Analog inputs

| Group | Count |
|---|---|
| Box pots | 24 |
| Header pots | 4 |
| Faders | 4 |
| Ribbons | 4 |
| Total | 36 |

No microcontroller has 36 analog inputs, so use analog multiplexers. Three **74HC4067** 16-channel muxes give 48 inputs on 3 analog pins plus 4 shared select lines. Wire each pot as a divider between 3.3 V and ground, wiper to a mux input.

**Ribbons need one extra resistor each.** A membrane ribbon is open circuit when untouched, so the mux input floats and reads noise. Put a 10 kΩ pull-down from the wiper line to ground. Untouched then reads near 0, which the firmware treats as "no finger". Touch reads the position. This gives the momentary behaviour for free.

### Digital inputs

| Group | Count |
|---|---|
| Rotary voice switches, 6 positions each | 4 |
| Toggles | 10 |

Two ways to read a 6-position switch. The simple way is six input pins per switch, 24 pins total, through a shift register such as a **74HC165** (three of them, daisy-chained, read on one SPI line). The neat way is one analog input per switch with a resistor ladder, six equal resistors between 3.3 V and ground and the wiper on the switch common, which turns each switch into a 6-step pot. The ladder uses 4 more mux inputs and no extra chips. Use the ladder.

Toggles go to 10 GPIO pins with internal pull-ups, or to one more 74HC165 if pins run out.

### Microcontroller

Any of these will do the whole job:

- **Raspberry Pi Pico** (RP2040). Cheap, 3 analog pins, plenty of GPIO, USB MIDI through the TinyUSB library. The sketch in `firmware/` targets this.
- **Teensy 4.0 or 4.1.** More analog pins, native USB MIDI in the Arduino menu, and it is also the board you would use for Phase 3 route B, so nothing is wasted.
- **Arduino Leonardo or Pro Micro.** Works, USB MIDI via the MIDIUSB library, but slow and short on pins.

### Scan rate and smoothing

Scan every input at about 200 Hz. Send a MIDI CC only when a value changes by more than one step, after a small moving average, so knobs don't chatter. Ribbons are the exception: send them at full rate while touched, and send 0 once when released.

## 4. The wire format: MIDI

USB MIDI is a standard the web page already understands through Web MIDI. Each control has a fixed CC number, listed in `CONTROL-MAP.md`. Each box uses its own MIDI channel, 1 to 4, and the header uses channel 5. The same CC numbers repeat on each box channel, which keeps the firmware trivial.

Switches send 0 or 127. Pots, faders and ribbons send 0 to 127. 7-bit resolution is fine for everything here except possibly the fader; if you want finer voice control, send the fader as a 14-bit CC pair. The map reserves the pair.

## 5. Phase 2 wiring and test plan

1. Wire one box's controls to the mux and the Pico on a breadboard. Load the sketch. Open a MIDI monitor and confirm each control sends its CC.
2. Add Web MIDI to `shruti-panel.html` so it maps incoming CCs to the same functions the on-screen controls call. On-screen controls stay live too, so you can compare.
3. Play it. Change the panel layout on paper until the hands are happy. Then cut the real panel.
4. Wire the remaining three boxes and the header.

## 6. Phase 3: making it standalone

**Route A, Raspberry Pi.** A Raspberry Pi 4 or 5 inside the case runs Chromium in kiosk mode showing `shruti-panel.html`, with the Pico plugged into it over USB. The Pi's audio jack or a small USB audio interface is the output. Zero porting. Costs a slow boot of about 20 seconds and a small screen if you want one, though the screen is optional: the panel is the interface.

**Route B, dedicated audio board.** Port `ENGINE.md` to C++ on a Teensy 4.1 with the Audio Shield, or on a Daisy Seed. Instant boot, low latency, no operating system. The panel wiring is identical to Phase 2, the microcontroller just runs the engine as well as reading the controls. This is the real instrument, and it is the most work. The engine spec is written to make the port mechanical: every filter, curve and constant is listed.

Start with Route A. Move to Route B only if boot time or latency bothers you in practice.

## 7. Audio output

Stereo line out on two 6.35 mm jacks or one 3.5 mm TRS. Width and Pan only mean something in stereo. A headphone jack is worth adding, since a drone is often played alone. Keep the master Level pot as the last thing before the output stage so it always works, even if software is wrong.

## 8. Power

The Pico route draws under 100 mA at 5 V, so USB power from the host is enough for Phase 2. For Phase 3 Route A, the Pi wants a proper 5 V 3 A supply. Route B on a Teensy is back under 200 mA.

## 9. What is deliberately not on the panel

- **No presets.** The panel is the sound. Photograph it if you want to remember a setting.
- **No lights.** State is always readable from the physical position of a part.
- **No screen.** The web page shows numbers for convenience while designing; the device does not.
- **No hold button.** Turning Pump all the way down is hold.
- **Only Fourth and Fifth.** The two intervals that cannot clash. Nothing else is offered.
