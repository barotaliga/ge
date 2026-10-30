---
type: Added
title: The classic pack's sounds enhanced to 48 kHz
---
The classicenhance pipeline gains a `sounds` stage: every sound in the player's own pack is read (8 or 16 bits, its loop start), its quantisation hiss reduced by a Wiener gain measured in its own quiet gaps, resampled to 48 kHz and its missing top band rebuilt from the real spectrum, then written to the local cache as 16-bit WAV with the loop start kept. The engine gains `audio::Fft` and the short-time spectrum (`audio::stft`, `audio::istft`) and `audio::resample`, a Kaiser-windowed sinc resampler.
