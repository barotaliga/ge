---
name: audio
description: "Use for engine/audio in gameengine2: the port of oss-game-framework's synthesized instrument (voice vocabulary, DSP graph, racks, play_def), game5's beds and bank, the audio thread, and parity against an offline WebAudio render."
---

# Audio

Every sound in game5 is synthesized. There are no audio files and no music.
`engine/audio/` ports the framework's `src/audio/` (voice, synth, rack, play, types,
view). `games/fall_line/app/audio/` ports game5's `pwa/src/game/audio/` (the bank,
the beds — wind, edges and snow, poles — and the listener table).

## Design

- `synth.ts` builds WebAudio node graphs. The port is a fixed-capacity DSP graph:
  oscillators (sine, square, saw, triangle), noise, biquad filters (WebAudio's
  formulas from the Audio EQ Cookbook), gain envelopes (`envelopeShape`), and the
  echo (delay, feedback, damping).
- It renders in blocks of 128 frames (WebAudio's render quantum) on the device thread.
  It does not allocate or lock there. The game thread sends parameter changes through
  a single-producer/single-consumer ring.
- Racks (steered layers) and beds read the `FrameView` once a frame.

## Parity

Exact bits are not required for audio. The check is that the sound is the same:
render a voice definition offline in the port and in WebAudio (a headless Chromium
`OfflineAudioContext` run by `tools/parity/`), then compare the RMS envelope and the
spectrum within stated tolerances. Put both meters in the PR.

## Rules

- `SoundDef` data stays the oracle's, ported field for field.
- One master and the two faders (game5's `mixOf`). Nothing else changes the volume.
