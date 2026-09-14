# Shruti 4

A four-box drone instrument that a newcomer cannot play wrong, built in plain HTML and Web Audio, on its way to becoming a physical device.

**Play it:** https://ianpilon.github.io/shruti-4/shruti-panel.html

![Shruti 4 panel](docs/shruti-panel.png)

Open any file directly in a browser. No build, no server, no dependencies.

## The instrument

- `shruti-panel.html` – the hardware-true panel. Four boxes, each a voice (shruti reed or one of five pad voices: Bed, Deep, Haze, Pulse, Glow) behind a bellows. Every control is a physical part: rotary voice switch, off/on and octave toggles, six knobs, a long fader and a squeeze ribbon per box; Fourth/Fifth and Pump Sync switches plus Age, Breath, Width and Level in the header. No presets, no lights: the panel is the sound.
- `shruti-panel-70s.html` – the same instrument in a brushed-aluminium and walnut tape-deck finish.
- `shruti-quartet-v2.html` – the tablet version, with presets and live readouts.

## How it got here

- `index.html` – a dependency-free rebuild of a classic online tone generator, with a live oscilloscope.
- `drones.html` – eight ways a tone becomes a drone, each a live patch.
- `shruti.html`, `shruti-mini.html`, `shruti-rocker.html`, `shruti-quartet.html` – the steps from a full shruti box to the four-box panel.
- `atmosphere.html` – a synthesized study of commercial ambient pad loops, built from spectral measurements rather than samples. The measured loops themselves are not in this repo.

Home is fixed at B2. The only intervals offered are the fourth and the fifth, so nothing can clash.
