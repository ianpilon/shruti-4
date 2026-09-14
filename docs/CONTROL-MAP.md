# Shruti 4 · Control map

Every control on the panel, what it does, its range, and its MIDI CC number. The web page and the hardware both follow this table. Values arrive as MIDI 0..127 and are scaled to the 0..1 fraction shown here, then to the engine value by the curve in the last column.

MIDI channels: Box 1 = channel 1, Box 2 = 2, Box 3 = 3, Box 4 = 4, header = channel 5. The CC numbers repeat on each box channel.

## Per box (channels 1 to 4)

| CC | Control | Part | Fraction → engine value | Notes |
|---|---|---|---|---|
| 20 | Voice | 6-position rotary switch | 0..5 → Reed, Bed, Deep, Haze, Pulse, Glow | Send 0, 25, 51, 76, 102, 127 for the six positions. |
| 21 | Off / On | toggle | 0 = off, 127 = on | Turning on starts the reeds with their onset; off closes them. |
| 22 | 1 OCT ↓ | toggle | 0 = normal, 127 = one octave down | Halves every voice frequency in the box. |
| 23 | Pump | pot | rate = (1/40) × 240^f strokes per second | Log scale from one stroke per 40 s to 6 per second. Below 1/36 per second the bellows is **held**: the bottom of the sweep is hold. Under Pump Sync, boxes 2 to 4 read this pot as a 6-step ratio instead: ½, ⅔, 1, 1½, 2, 3 × Box 1's rate. |
| 24 | Drive | pot | d = f | 0 to 0.5 soft warmth, 0.5 to 1 hardening into a clip. Pre-gain 1 + (9d + 30d³) × (0.35 + 0.85p) where p is air pressure. |
| 25 | Grit | pot | g = f | Wavefold amount g², presence +14g dB at 3 kHz, tone ceiling × (1 + 1.6g). |
| 26 | Room | pot | send = f | Send level into the shared reverb. |
| 27 | Level | pot | gain = f² | Squared so the knob feels even. |
| 28 | Pan | pot, centre detent | pan = 2f − 1 | −1 left, +1 right. Default spread L60, L20, R20, R60. |
| 29 | Voice fader | 100 mm slide pot | v = f | 0 = dark and soft, 1 = bright and full. Sets air centre, tone ceiling, breath noise, and the balance of the upper voices. |
| 30 | Squeeze ribbon | 100 mm ribbon | sq = f while touched, 0 on release | Momentary. Adds up to 0.55 to the air pressure target, on top of the pump or a held pressure. |
| 61 / 93 | Voice fader, 14-bit (optional) | | MSB on 61, LSB on 93 | Use instead of CC 29 if 128 steps feels coarse on the fader. |

## Header (channel 5)

| CC | Control | Part | Fraction → engine value | Notes |
|---|---|---|---|---|
| 40 | Fourth / Fifth | toggle | 0 = fourth (4:3), 127 = fifth (3:2) | Retunes the interval voices of all four boxes at once, with a 150 ms close and reopen so reeds don't glide. |
| 41 | Pump Sync | toggle | 0 = free, 127 = locked | Box 1 becomes the clock. Boxes 2 to 4 snap to the nearest ratio and their pulses are phase-aligned to Box 1. If Box 1 is held, all four hold. |
| 42 | Age | pot | drift = 25f cents; box offsets 0, +3.5, −2.5, +5 cents × (f / 0.42) | Default f = 0.42, giving 10.5 cents of drift. "New" at 0 is a sterile locked tone; 25 cents is old swimming boxes. |
| 43 | Breath | pot | depth multiplier = 2f | How deep every pump stroke swells. Default 0.5 (×1). At 0 the tone barely moves while pumping. |
| 44 | Width | pot | split = ±12f cents | Each voice is a left and right oscillator pair split by this much, each with its own slow drift. Default 0.5 (±6 cents). |
| 45 | Level | pot | master gain = 0.9 f² | Default f = 0.707 (gain 0.45). |

## Fixed values, not on the panel

| Item | Value |
|---|---|
| Home pitch | B2, 123.47 Hz |
| Room (reverb size) | 0.45 → impulse 3.5 s, decay exponent 2.57 |
| Ribbon squeeze gain | 0.55 of full pressure |
| Hold threshold | Pump rate ≤ 1/36 strokes per second |

## Hardware notes on specific controls

- **Pump under sync.** The pot doesn't move when Pump Sync flips; its sweep is simply divided into six zones. Print two scales around the Pump knobs on boxes 2 to 4: an outer speed scale for free running and an inner six-mark ratio scale for sync. Box 1's Pump gets only the speed scale and a small "clock" legend.
- **Ribbon.** Pull the wiper down with 10 kΩ so an untouched ribbon reads 0. Send at full scan rate while touched, then a single 0 on release. The engine eases the squeeze in at 9/s and out at 3/s, so raw ribbon jitter is smoothed on the receiving side.
- **Voice switch.** Send the new position once on change. The engine crossfades voices over about 0.6 s and rebuilds oscillators, so bouncing contacts should be debounced for 20 ms in firmware.
- **Toggles.** Debounce 20 ms, send once per change.
