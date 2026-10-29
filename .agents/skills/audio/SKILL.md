---
name: audio
description: "Use for engine/audio in gameengine2 and any sound: the synth (tones, noise, steered layers, sampled voices from WAV files, buses, echo, limiter, the render ring), spatial sound and the channel limit (channels.hpp), SoundDef and play_def, racks and beds, the meter and WAV, the audio device, Fall Line's bank and beds, and checking a sound by its meters (the synth and audition labs) when nobody can listen."
---

# Audio

Fall Line's every sound is synthesized: it ships no audio files and no music. A game that ships
recordings (Shooter's Edge) plays them on the same instrument as SAMPLED VOICES (below), through
the same buses, echo, room and limiter. `engine/audio/` is the instrument; a game's sound (its bank, its beds, its listener table, its faders) is the game's,
and the beds a genre shares go in that plugin's `app/` half (`plugins` skill).

## What is built (M9a)

| File | What |
| --- | --- |
| `voice.hpp` | The vocabulary: `Tone`, `Noise` (one-shots), `LayerSpec`, `LayerTarget` (steered layers), `kRate` (48 kHz, everywhere), the envelope floor `kMinAttackMs`, `safe_cutoff`, the shaper's rules |
| `dsp.hpp` | Per-sample blocks: the sine table, the saturation curves, the noise pools, polyBLEP oscillators, the state-variable filter (`svf_tune` every `kControl` samples), one-pole glides |
| `synth.hpp` | The graph: `tone`, `noise`, `layer`/`set`/`stop`, `bus_gain`, `render`; the ring, the voice pool, the echo, the limiter, `stats`; STREAMS (`stream`, `feed`, `queued`, `steer`, `stop`): mono samples made elsewhere (voice chat's talkers, `engine/net/voice.hpp`) on a lock-free ring each, played on a bus, `Stats::starved` when one runs dry |
| `sound.hpp` | `SoundDef` (id, the sentence it should feel like, its voices), `PlayShape` (gain, pitch, stretch, pan), `play_def`, `find_sound` |
| `rack.hpp` | A bed's layers: built on the first `rack_apply`, steered every frame |
| `pcm.hpp` | `Pcm` (mono samples at their own rate, the loop) and `read_wav` (8/16-bit PCM, float, extensible, the loop from `smpl`, `cue ` + `ltxt`, or `cue ` alone) |
| `synth.hpp` (`sample`) | Sampled voices: a `Pcm` read in 32.32 fixed point with Catmull-Rom interpolation, pitched, looped (`Loop::file`, `once`, `whole`), steered (`sample_set`), stopped with a fade (`sample_stop`), `kMaxSamples` at once, the quietest stolen; `sample_live` from the producer |
| `channels.hpp` | Sounds in a world: `hear` (gain falling linearly with distance, pan off the listener's right) and `Channels` (an entity's channel replaces, channel 0 stacks, the limit steals the quietest, the inaudible never start, the listener's own centred, `move` and `update` once a frame) |
| `meter.hpp`, `wav.hpp` | A buffer's level, peak, octave bands, centroid, onset, length and balance; its WAV bytes |
| `engine/platform/audio.hpp` | The device: `open_audio` pulls the synth on SDL's audio thread; `open_audio_in` and `read_audio_in` the microphone |
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

- **`samples`** (`engine/lab/labs/audio_samples_lab.cpp`): recordings at three rates, three
  pitches, a cue's loop and its stop, a fly-by (its gain and pan charted, its balance metered)
  and sixteen sources into eight channels; the bench times a full pool.
- **`se-audio`** (`games/shooters_edge/labs/se_audio_lab.cpp`): a scripted fight of the game's
  WAVs through `Channels`: the fight from above, each source's heard gain, the mix and its WAV.
- **`se-ambient`** (`games/shooters_edge/labs/se_ambient_lab.cpp`): a walk through the fixture
  halls (sky in the hall, water in the side room) with the game's ambience by leaf: the walk
  from above, each layer's target and level, the mix and its WAV; fails a level faster than the
  rate, a silent layer holding a voice, a clip.
- **`synth`** (`engine/lab/labs/audio_synth_lab.cpp`): every wave, colour, sweep, drive, the pair
  and vibrato, the echo, two steered layers and the limiter under a stack, each metered,
  pictured and written as WAV, with failures for the checks a listener would make; the bench.
- **`audition`** (`games/fall_line/labs/audition_lab.cpp`, `--scene=all|bank|beds|run`): every
  sound in Fall Line's bank, each held to the bank's band (its loudest between -40 and -16
  dBFS) and to its sentence; the skier's beds at seven moments (stand, cruise, tuck, carve,
  skid, powder, air), with failures for the order a listener hears (the tuck over the cruise,
  the powder darker than the groomer, the skid brighter than the powder, silence at rest); and
  the bot's whole run on a seed through the route, under the limiter with no dropped call.

## Sampled voices (M18j)

- **A `Pcm` is read in place, never copied.** The game loads its bank once and keeps it alive as
  long as the synth; a call carries a pointer across the ring.
- **A sampled voice is steered, not re-fired.** A moving source keeps its voice and gets a
  `sample_set` each frame (`Channels::update` does it); the 0.02-0.03 s glide is what keeps a
  pan moving at 72 Hz from zippering.
- **`sample_live` is safe before the first render**: an id sent and not yet applied counts as
  live, so a channel table never frees a slot whose voice has not started.
- **`hear`'s law is linear on purpose** (silent at 1 / attenuation): a channel table can then
  say exactly what is out of earshot and never start it.
- Labs: `samples` (the mechanism, synthetic recordings) and `se-audio` (a scripted fight of
  Shooter's Edge's own WAVs; the fight's balance checks: the rocket's blast and the beam left,
  the door right).
- **An ambience is a bed, not a placed sound** (`audio/ambient.hpp`): its layers are not panned
  or attenuated, and a level walks to its target at a fixed rate, so crossing a leaf swells or
  fades it. A target under the bed's floor is silence and a layer at zero frees its voice, so a
  map that is quiet costs no voices. Shooter's Edge sets the targets off the listener's leaf
  (`games/shooters_edge/sound/ambience.hpp`); only the 29 family and BSP2 store leaf levels.

## Fall Line's sound (where each piece is)

- **The beds** (`plugins/skier/app/`): `wind_bed` (rush, roar, whistle, flutter, buffet off the
  air, the tuck and a crosswind), `snow_bed` (hiss, powder hush, edge tear, chatter, skid rasp
  off the packed share, the edge, the skid and the pair), `ride_bed` (the skier's state turned
  into both voices and steered on two racks). Pure functions of the moment; the rack steers.
- **The game's** (`games/fall_line/sound/`, the `fall_line_sound` target, no GPU): `bank` (every
  one-shot as data), `route` (racer 0's events to the bank per step, the bed per frame),
  `listener` (one row per camera seat), `mix` (the faders folded into the two buses).
- **THE AIR HEARD is the skier's own way** until the sim carries a weather wind: the wind voice
  follows his speed on a quarter second.
- **Events are per step** (the state's list is cleared every step), so `run_sound_step` runs
  after every step and `run_sound_frame` once a frame; a layer stopped fades in 0.02 s and is
  freed after 0.15 s, so a test counts layers only after that much render.
