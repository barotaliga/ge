// A RENDERED BUFFER AS A WAV FILE: 16-bit PCM stereo at the synth's rate, the bytes of the
// whole file. A lab attaches them beside its pictures so a person on a machine with speakers can
// listen to exactly what was metered. Each sample is clamped to [-1, 1], scaled by 32767 and
// rounded halves toward +inf (`core::num::round`), so the bytes are as deterministic as the
// buffer.
//
// ge:summary A rendered buffer as WAV bytes: 16-bit PCM stereo, 48 kHz
// ge:keywords wav wave riff pcm file export listen encode wav_bytes
// ge:skill audio labs
// ge:test tests/audio_meter_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ge::audio {

std::vector<uint8_t> wav_bytes(std::span<const float> stereo);

}  // namespace ge::audio
