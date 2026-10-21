---
name: audio
description: "Use for engine/audio in gameengine2: the synthesized instrument (voice vocabulary, DSP graph, racks, play_def), Fall Line's beds and bank, the audio thread, and checking a sound against a reference render by meter."
---

# Audio

Every sound in Fall Line is synthesized. There are no audio files and no music.
`engine/audio/` is the instrument (voice, synth, rack, play, types, view).
`games/fall_line/app/audio/` is the game's sound (the bank, the beds — wind, edges and
snow, poles — and the listener table). While porting, `porting/subsystems.md` § audio
names the reference modules.

## Design

- The synth is a fixed-capacity DSP graph:
  oscillators (sine, square, saw, triangle), noise, biquad filters (the
  Audio EQ Cookbook's formulas), gain envelopes, and the
  echo (delay, feedback, damping).
- It renders in blocks of 128 frames on the device thread.
  It does not allocate or lock there. The game thread sends parameter changes through
  a single-producer/single-consumer ring.
- Racks (steered layers) and beds read the `FrameView` once a frame.

## Checking a sound

Exact bits are not required for audio. The check is that the sound is the same: render a
voice definition offline and compare its RMS envelope and spectrum with a reference render
within stated tolerances (while porting, the reference render is made under `porting/`).
Put both meters in the PR.

## Rules

- A `SoundDef` is plain data, one field per parameter the graph reads.
- One master and the two faders (`mix_of`). Nothing else changes the volume.

## Labs

M9 lands `audition`: render each voice and bed to a buffer offline and meter it (level,
spectrum, envelope) as metrics, with its waveform as a captioned picture (the `labs` skill).
