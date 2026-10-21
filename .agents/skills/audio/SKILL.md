---
name: audio
description: "Use for engine/audio in gameengine2 and any sound: the synth (tones, noise, steered layers, buses, echo, limiter, the render ring), SoundDef and play_def, racks and beds, the meter and WAV, the audio device, Fall Line's bank and beds, and checking a sound by its meters (the synth and audition labs) when nobody can listen."
---

# Audio

Every sound is synthesized. There are no audio files and no music. `engine/audio/` is the
instrument; a game's sound (its bank, its beds, its listener table, its faders) is the game's,
and the beds a genre shares go in that plugin's `app/` half (`plugins` skill).

## What is built (M9a)

| File | What |
| --- | --- |
| `voice.hpp` | The vocabulary: `Tone`, `Noise` (one-shots), `LayerSpec`, `LayerTarget` (steered layers), `kRate` (48 kHz, everywhere), the envelope floor `kMinAttackMs`, `safe_cutoff`, the shaper's rules |
| `dsp.hpp` | Per-sample blocks: the sine table, the saturation curves, the noise pools, polyBLEP oscillators, the state-variable filter (`svf_tune` every `kControl` samples), one-pole glides |
| `synth.hpp` | The graph: `tone`, `noise`, `layer`/`set`/`stop`, `bus_gain`, `render`; the ring, the voice pool, the echo, the limiter, `stats` |
| `sound.hpp` | `SoundDef` (id, the sentence it should feel like, its voices), `PlayShape` (gain, pitch, stretch, pan), `play_def`, `find_sound` |
| `rack.hpp` | A bed's layers: built on the first `rack_apply`, steered every frame |
| `meter.hpp`, `wav.hpp` | A buffer's level, peak, octave bands, centroid, onset, length and balance; its WAV bytes |
| `engine/platform/audio.hpp` | The device: `open_audio` pulls the synth on SDL's audio thread |
| `engine/lab/sound.hpp` | `sound_shot`: a buffer metered, its metrics, its picture, its WAV, in a lab |

## Design rules

- **The render never locks and never allocates.** The game thread calls; the calls cross a
  single-producer single-consumer ring (`kRingSize`) and are applied at the next block. A full
  ring drops and counts (`Stats::dropped`), a full pool steals the oldest one-shot
  (`Stats::stolen`). Everything is sized in the constructor.
- **Deterministic to the bit.** float arithmetic under the engine's FP flags, every
  transcendental through `core/libm` (in a table or a per-control-step coefficient, never per
  sample), noise from `core::Rng`. `make libm-check` scans `engine/audio`. The same calls before
  the same renders give the same bytes on every CI row, so `tests/golden/audio_synth.txt` holds
  render hashes and the labs' digests include WAV bytes.
- **A continuous sound is a layer, never a train of one-shots.** A bed is a rack: specs, glides
  and a pure function of the frame's state to targets. A late frame leaves a layer holding.
- **The faders are buses.** A voice or layer plays on one of `kBuses`; `bus_gain` is applied as it
  renders, so a fader moved mid-sound is heard at once. The game's `mix_of` folds its master and
  switches into the bus gains; nothing else changes the volume.
- **A `SoundDef` is plain data**, one field per parameter the graph reads, `constexpr` in the
  game's bank. A play scales it (`PlayShape`); a new sound never needs code.
- **Filters are state-variable**, not direct-form biquads, because a cutoff that moves every few
  samples (a sweep, a steered layer) stays stable. `q` is the resonance (0: 0.707, or 1 for a band
  pass, which peaks at unity). A level, a cutoff or a pan is never stepped: everything moves on a
  glide or a ramp, and every onset has at least `kMinAttackMs`.

## Checking a sound without ears

Bit parity with any other implementation is not a goal. The check is the sound's SHAPE, read off
its meters and its picture:

1. `make lab LAB=synth ARGS=--tag=before` (or the game's `audition`) before the change.
2. Change it, then `make lab LAB=synth ARGS=--compare=before`: every `<id>.loudest-db`,
   `.centroid-hz`, `.length-s` beside the old one. Read `previews/<lab>/index.md` and look at
   the pictures that moved; each says what to look for.
3. A change to the DSP moves `tests/golden/audio_synth.txt`: rerun the test with
   `GE_GOLDEN_WRITE=1`, run `make labs-digests`, and put both labs' compare tables in the PR.
4. The WAVs sit beside the pictures for a person with speakers. A listening test only the Mac can
   do is a `needs-mac` issue (AGENTS.md), never a reason to skip the meters.

What the meters can say: how loud it gets (the loudest 10 ms, dBFS; a bank lives at about -34 to
-20), where its energy is (the bands and the centroid: brown under ~1 kHz, white near 8 kHz),
when it starts and how long it rings, and which side it is on.

## Cost

The `synth` lab's bench renders a busy race's load (24 layers, ~16 one-shots) and reports
`bench.realtime-share`, measured: about 1.7 % of one core on PRESET=release (M9a). Time it on
release; a debug build times the sanitizers. A performance change owes that number before and
after (`perf` skill).

## Labs

- **`synth`** (`engine/lab/labs/audio_synth_lab.cpp`): every wave, colour, sweep, drive, the pair
  and vibrato, the echo, two steered layers and the limiter under a stack, each metered,
  pictured and written as WAV, with failures for the checks a listener would make; the bench.
- **`audition`** (M9b, `games/fall_line/labs/`): every sound in Fall Line's bank and every bed
  preset, metered the same way.
