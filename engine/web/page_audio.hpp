// THE PAGE'S SOUND (Emscripten builds only, GE_WEB; docs/hub.md § Playing in a browser): the
// engine's synth (engine/audio) heard through the browser's Web Audio. Each paint the game's synth
// renders the frames that keep the page a little ahead of what it is playing (`audio_pump`), and
// they are queued on the page's audio context back to back, at the synth's own rate (48 kHz
// stereo), so the browser resamples once to its output.
//
// A browser lets a page sound only after a click or a key on it: until then the context waits,
// and the synth is rendered and thrown away at the paint's pace, so the game's sounds stay in
// time and the first one heard is the one playing now, not a backlog. The page's first click
// (which also locks the mouse to the canvas to play) starts it.
//
//   audio_open();                         once, when the game starts
//   every paint: audio_pump(synth, dt);   after the game's frame queued its sounds
//
// ge:summary The page's sound: the engine's synth rendered each paint and queued on Web Audio
// ge:keywords web audio web-audio audio-context sound synth audio_pump autoplay page-sound
// ge:skill net audio
#pragma once

#include "audio/synth.hpp"

namespace ge::web {

// The page's audio context made; false when the browser has none (the game then plays silent).
bool audio_open();
// Whether the page's audio plays (it has had its click).
bool audio_playing();
// `synth`'s next frames rendered onto the page, `lead_s` ahead of what plays (more after a slow
// paint: `dt` and a half, at most a quarter second); before the page plays, `dt` seconds of them
// rendered and dropped.
void audio_pump(ge::audio::Synth& synth, double dt, double lead_s = 0.1);

}  // namespace ge::web
