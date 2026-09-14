# Shruti 4 · Hardware build guide

This is the plan for building Shruti 4 as a standalone physical instrument. `shruti-panel.html` is the reference design and the test bench, not a product: every control on it is a real part, nothing lights up, and there are no presets. The panel is the sound. The finished device has no screen and no computer you interact with.

Companion documents:

- `CONTROL-MAP.md` – every control, its range, its curve, and its MIDI CC number.
- `ENGINE.md` – the sound engine, written so it can be ported line for line.
- `firmware/shruti4_controller/shruti4_controller.ino` – a starter sketch that turns the panel into a USB MIDI controller.

## 1. Architecture: three layers inside one box

The design is split so the panel and the sound never have to know about each other.

```
 control surface  ──►  parameter table  ──►  sound engine  ──►  line out
 (knobs, faders,       (one number per        (oscillators, bellows,
  switches, ribbons)    control, 0..1)         drive, reverb)
```

All three layers live inside the instrument. The build goes in two stages, both of which end with the same panel:

- **Stage 1, the panel on the bench.** Wire the real controls to the controller board and confirm every one of them reads correctly. During this stage the reference web page is the sound engine, fed over a USB cable, purely so you can hear the panel while the embedded engine is being written. That is a test rig, not a product.
- **Stage 2, the instrument.** The engine runs on an audio board inside the case, the USB cable goes away, and the box plays from its own line out the moment it is powered.

Section 6 covers the two ways to do Stage 2.

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

- **Teensy 4.1.** The recommended board, because it is also the board that runs the engine in Stage 2, so the bench wiring carries straight into the instrument. Native USB MIDI in the Arduino menu, plenty of analog and digital pins.
- **Raspberry Pi Pico** (RP2040). Cheaper for a first breadboard, USB MIDI through the TinyUSB library. The sketch in `firmware/` builds for either.
- **Arduino Leonardo or Pro Micro.** Works, USB MIDI via the MIDIUSB library, but slow and short on pins.

### Scan rate and smoothing

Scan every input at about 200 Hz. Send a MIDI CC only when a value changes by more than one step, after a small moving average, so knobs don't chatter. Ribbons are the exception: send them at full rate while touched, and send 0 once when released.

## 4. The wire format: MIDI

USB MIDI is a standard the web page already understands through Web MIDI. Each control has a fixed CC number, listed in `CONTROL-MAP.md`. Each box uses its own MIDI channel, 1 to 4, and the header uses channel 5. The same CC numbers repeat on each box channel, which keeps the firmware trivial.

Switches send 0 or 127. Pots, faders and ribbons send 0 to 127. 7-bit resolution is fine for everything here except possibly the fader; if you want finer voice control, send the fader as a 14-bit CC pair. The map reserves the pair.

## 5. Stage 1: bench wiring and test plan

1. Wire one box's controls to the mux and the controller on a breadboard. Load the sketch. Open a MIDI monitor and confirm each control sends its CC.
2. Plug the controller into a laptop running `shruti-panel.html` with Web MIDI enabled, so the physical controls drive the reference engine. This is only to hear the panel before the embedded engine exists.
3. Play it. Change the panel layout on paper until the hands are happy. Then cut the real panel.
4. Wire the remaining three boxes and the header.
5. Move to Stage 2. The laptop is never part of the finished instrument.

## 6. Stage 2: the engine inside the box

**Route A, dedicated audio board. This is the target.** Port `ENGINE.md` to C++ on a Teensy 4.1 with the Audio Shield, or on a Daisy Seed. The same board reads the panel and runs the engine, so the controller sketch and the engine become one program. Instant boot, latency under 2 ms, no operating system, no screen, nothing to update. The engine spec lists every filter, curve and constant so the port is mechanical rather than creative. Budget: the port is the largest single piece of work in the whole build.

**Route B, embedded Linux, a fallback.** A Raspberry Pi inside the case boots straight into a headless browser running `shruti-panel.html`, with the controller board on an internal USB cable and a USB audio interface for the output. No screen, no keyboard; from the outside it is identical to Route A. It avoids the port entirely at the cost of a 20 second boot and a general-purpose computer living in the box. Use it if you want to play the finished panel before the port is done, then replace the Pi with the audio board later. The panel and the controller wiring do not change.

## 7. Audio output

Stereo line out on two 6.35 mm jacks or one 3.5 mm TRS. Width and Pan only mean something in stereo. A headphone jack is worth adding, since a drone is often played alone. Keep the master Level pot as the last thing before the output stage so it always works, even if software is wrong.

## 8. Power

On the bench the controller draws under 100 mA at 5 V, so USB power from the laptop is enough. The finished instrument on a Teensy or Daisy draws under 200 mA: a 5 V wall adapter or a USB-C power input is fine. The Pi fallback wants a proper 5 V 3 A supply.

## 9. What is deliberately not on the panel

- **No presets.** The panel is the sound. Photograph it if you want to remember a setting.
- **No lights.** State is always readable from the physical position of a part.
- **No screen, no computer to interact with.** The web page shows numbers for convenience while designing; the device shows nothing.
- **No hold button.** Turning Pump all the way down is hold.
- **Only Fourth and Fifth.** The two intervals that cannot clash. Nothing else is offered.
