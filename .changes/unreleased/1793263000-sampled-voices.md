---
type: Added
title: Recorded sounds, heard from where they happen
---

The synth plays recordings beside its own voices: `audio::read_wav` reads WAV files (8- and 16-bit PCM, float, any rate, mono or mixed down) with their loop cue, and `Synth::sample` plays them at their own rate with cubic interpolation, pitched, looped, steered and stopped with a fade, 32 at once. `audio::Channels` places them in a world: a linear fall with distance, the pan from the listener's right, an entity's channel replacing its last sound, a channel limit that steals the quietest, and the listener's own sounds centred. New labs: `samples` and `se-audio` (a scripted fight of Shooter's Edge's sounds, heard by the player).
