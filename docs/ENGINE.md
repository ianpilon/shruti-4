# Shruti 4 · Sound engine specification

This describes exactly what `shruti-panel.html` computes, so the engine can be ported to C++ on a Teensy, Daisy or similar with the same result. Everything below is taken from the working code. Sample rate is whatever the platform runs; the web version runs at 48 kHz.

Notation: `p` is a box's air pressure (0..1.05), `v` its voice fader (0..1), `f` a knob fraction (0..1). "Smooth to X over T" means an exponential approach with time constant T, the equivalent of Web Audio's `setTargetAtTime`.

## 1. Global

| Name | Value |
|---|---|
| Home frequency | 123.47 Hz (B2) |
| Interval ratio I | 3/2 when the switch is on Fifth, 4/3 on Fourth |
| Age → drift cents | 25 × age (default age 0.42 → 10.5 ¢) |
| Age → box offsets | Box 1..4: 0, +3.5, −2.5, +5 cents, each × (age / 0.42) |
| Width → split | ±12 × width cents between a voice's left and right oscillator (default ±6 ¢) |
| Breath → depth multiplier | 2 × breath (default ×1) |
| Master level | 0.9 × level² (default 0.45) |

## 2. Voices

A box plays one voice at a time. A voice is a list of slots; each slot is a frequency ratio to home and a relative level. Ratios use `I` for the interval chosen by the switch.

| Voice | Wave | lp scale | Attack | Root | Gain | Slots (ratio : level) |
|---|---|---|---|---|---|---|
| Reed | reed | 1.00 | 0.28 s | 1 | 1 | 1 : dyn, I : dyn, 2 : dyn |
| Bed | dark | 0.52 | 2.0 s | 2 | 2.0 | 2 : 1, I : 0.45, 4 : 0.65, 2I : 0.5, 4I : 0.2 |
| Deep | dark | 0.18 | 2.0 s | 1 | 1.1 | 1 : 1, I/2 : 0.7, 1/2 : 0.35, I : 0.5, 2 : 0.3 |
| Haze | dark | 0.30 | 3.0 s | 2 | 2.2 | 2 : 1, I : 0.3, 2I : 0.5, 4 : 0.3, 9/2 : 0.08 |
| Pulse | dark | 0.14 | 1.5 s | 1 | 1.1 | 1 : 1, I/2 : 0.5, I : 0.45, 2 : 0.3 |
| Glow | dark | 3.20 | 2.5 s | 2 | 1.6 | 2 : 1, 2I : 0.6, 4 : 0.85, 4I : 0.55, 8 : 0.6, 8I : 0.3 |

Slot frequency = home × ratio × (0.5 if 1 OCT ↓ is on).

**Slot level.**
- Reed: slot `1` = 0.16; slot `2` = 0.03 + 0.14v; slot `I` = 0.09 + 0.07v.
- Pads: 0.17 × gain × level × (ratio > root × 1.6 ? 0.5 + 0.7v : 1). The fader lifts the upper voices.

**Waveforms**, given as harmonic amplitudes for an additive or wavetable oscillator:
- reed: 64 harmonics, amplitude 1/n^1.05; harmonics 3 to 7 multiplied by 1.35; above harmonic 30 multiplied by 0.9^(n−30). Sine phases, all harmonics.
- dark: 24 harmonics, amplitude 1/n^1.7.

Both are normalised to unit peak.

### Each slot is a stereo pair

Every slot runs two oscillators, one per output channel:
- Left detune = box offset − split, right detune = box offset + split (cents).
- Each oscillator also has its own slow drift: a sine LFO at frequency 1 / (15 + 6.3·slotIndex + 2.7·boxIndex + 4.1·side) Hz, amplitude = drift cents, added to detune.
- Both sides add the shared pressure bend (section 4).

### Onset and release

- Opening a slot: gain ramps linearly from 0 to its level over the voice's attack time. The reed additionally starts 25 cents flat and ramps to pitch over 0.35 s (the reed "speaks").
- Closing: reed ramps to 0 over 0.18 s, pads over 0.8 s.
- Changing the interval switch: slots containing `I` ramp to 0 over 0.15 s, retune, then reopen over min(attack, 1.2 s). Reeds do not glide.
- Changing voice: all old slots ramp to 0 over 0.6 s and are destroyed 0.8 s later; the new voice opens with its attack.
- Level changes from the fader: ramp to the new level over 0.12 s.

## 3. The bellows

Each box has a pressure `p` that follows a target with an asymmetric response, updated every frame (60 Hz in the web version; run it at control rate).

```
rate      = pump rate in strokes/s (see Pump curve below)
phase    += dt × rate
s         = sin(2π × phase)
depth     = (0.22 + 0.12 × min(1, rate / 2)) × 2 × breath
centre    = 0.38 + 0.45 × v
squeeze   = eased ribbon value (approach 9/s rising, 3/s falling)
target    = min(1.05, centre + depth × sign(s) × |s|^0.6 + 0.55 × squeeze)
k         = target > p ? max(4.5, 9 × rate) : max(1.1, 5 × rate)
p        += (target − p) × min(1, k × dt)
```

- Box off: target = 0.
- **Held** (Pump at or below 1/36 strokes/s): on entering hold record `base = p − 0.55 × squeeze`; while held, target = min(1.05, base + 0.55 × squeeze). Leaving hold resumes the stroke.
- **Pump curve:** rate = (1/40) × 240^f, where f is the pot fraction. Range 1/40 to 6 strokes per second.
- **Pump Sync:** Box 1's rate is the clock. Boxes 2 to 4 use rate = clock × ratio, ratio ∈ {½, ⅔, 1, 1½, 2, 3} chosen by their Pump pot in six equal zones. When sync engages or a ratio changes, set the follower's phase = clock phase × ratio so pulses align. If the clock is held, followers are held.

## 4. What pressure does

Applied each frame, all smoothed:

| Target | Value | Smoothing |
|---|---|---|
| Pressure gain | p^1.35 | 30 ms |
| Tone low-pass cutoff | ((700 + 2200p) × (0.35 + 0.65v) + 400v) × voice.lp × (1 + 1.6 × grit) | 50 ms |
| Breath noise gain | (0.012 + 0.03v) × p | 50 ms |
| Pitch bend, all slots | −9p cents | 50 ms |
| Drive pre-gain | 1 + (9d + 30d³) × (0.35 + 0.85p) | 50 ms |
| Drive post-gain | pre^−0.62 | 50 ms |

## 5. Per-box signal chain

```
slots (stereo sum)
  → pre-gain
  → drive shaper            tanh(1.6x)/tanh(1.6) blended toward clip(2.5x) as drive goes 0.5 → 1
  → post-gain
  → grit shaper             x·(1−g²) + sin(x·(1+6g)·π/2)·g²
  → presence                peaking EQ, 3000 Hz, Q 0.9, gain 14g dB
  → tone low-pass           2-pole, Q 0.3, cutoff from section 4
  → pressure gain
  → level gain              level²
  → pan                     equal-power stereo pan, −1..1
  → body (shared)
  → reverb send             gain = room, taken after pan
```

Breath noise: white noise → band-pass 2600 Hz, Q 0.7 → noise gain → into the pressure gain stage (it rides the same bellows).

**Shared body**, after the four boxes are summed:
```
peaking 620 Hz, Q 1.6, +5 dB
peaking 1750 Hz, Q 2.2, +4 dB
high-pass 70 Hz
master gain (0.9 × level²)
```

**Reverb**, one for the whole instrument: convolution with a synthetic stereo impulse, length 0.6 + 6.5 × 0.45 = 3.5 s, white noise × (1 − t/T)^2.57, then low-pass 4500 Hz, then gain 0.9, into the master. On a microcontroller replace the convolution with any decent algorithmic hall of about 3.5 s decay and a 4.5 kHz damping; the exact tail is not critical.

Drive and grit shapers run with 2× oversampling in the web version.

## 6. Constants you will want to keep

| Constant | Value |
|---|---|
| Reed slot base level | 0.16 |
| Pad slot base level | 0.17 × voice gain |
| Reed onset flat start | −25 cents over 0.35 s |
| Squeeze contribution | 0.55 of full pressure |
| Pressure ceiling | 1.05 |
| Drift LFO period | 15 to ~50 s depending on slot, box and side |
| Grit presence | up to +14 dB at 3 kHz |
| Tone ceiling scale by voice | Reed 1, Bed 0.52, Deep 0.18, Haze 0.30, Pulse 0.14, Glow 3.2 |

## 7. What to leave out when porting

The web page carries a few things that only exist for the screen: the numeric readouts (already hidden), the finger marker on the ribbon, and the amber held mark on the fader cap. None of these affect the sound.
