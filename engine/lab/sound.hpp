// A SOUND IN A LAB: a rendered buffer metered (`audio/meter.hpp`), its numbers written as the
// lab's metrics, its picture drawn (the waveform, its 10 ms level envelope and its octave bands)
// and its WAV attached beside it, in one call. Every audition lab, the engine's and a game's,
// shows a sound this way, so a reader learns one picture.
//
// THE PICTURE, left to right: the waveform as each column's range (left channel up, right
// channel down, on one axis, zoomed so its peak fills the half), the level envelope over it as a
// line from -80 to 0 dBFS, and on the right the ten octave bands as bars on the same dB scale.
//
// ge:summary A sound in a lab: metered, its metrics, its captioned picture and its WAV
// ge:keywords lab sound audition waveform envelope spectrum bands plot meter wav sound_shot
// ge:skill labs audio
// ge:test tests/audio_meter_test.cpp
#pragma once

#include <span>
#include <string>

#include "audio/meter.hpp"
#include "lab/lab.hpp"

namespace ge::lab {

// Meter `stereo`, write `<shot.id>.loudest-db`, `.peak-db`, `.centroid-hz`, `.onset-s` and
// `.length-s`, add the captioned picture, attach `<shot.id>.wav`, and return the meter.
audio::Meter sound_shot(Lab& lab, Shot shot, std::span<const float> stereo);

// The picture alone.
Picture sound_picture(const audio::Meter& meter, std::span<const float> stereo);

}  // namespace ge::lab
