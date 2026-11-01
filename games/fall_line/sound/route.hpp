// THE RUN'S SOUND: the event route and the ride bed together, for one app. Two calls, at two
// rates, because the simulation and the picture run at two:
//
//   run_sound_step   after every simulation step: this step's events become the bank's
//                    one-shots (the events are cleared at the next step, so a frame that ran
//                    three steps routes three times)
//   run_sound_frame  once a rendered frame: the player's ride bed steered to where he is now
//
// WHAT IS HEARD is the PLAYER's run (racer 0): his gates, his landings, his trunks and his falls,
// and any shoulder he is part of. A rival's own events are not routed; the field is heard when it
// meets him. The snow under a landing picks its sound (deep loose snow is the whumpf, past half the
// legs' limit it is the hard landing), and the load scales it.
//
// A THUNDERSTORM over the map (mountain/air's `storm_of`, `Weather::thunder` above 0) is heard at
// the player's skis once a frame (engine/wind/thunder_play.hpp): every flash's thunder queued at
// the moment its first sound reaches him and started on the effects bus, unless `thunder` is off
// (cfg's s_thunder). A clock gone back (a restart, a rewind) starts the hearing over.
//
// ge:summary The run's sound: events routed to the bank each step, the ride bed steered each frame
// ge:keywords route events sound run_sound landing wipeout gate finish bed listener step frame
// ge:skill audio sim
// ge:test tests/fall_line_sound_test.cpp
#pragma once

#include <string_view>

#include "audio/synth.hpp"
#include "core/prng.hpp"
#include "fall_line/sim/state.hpp"
#include "fall_line/sound/listener.hpp"
#include "skier/app/ride_bed.hpp"
#include "wind/storm.hpp"
#include "wind/thunder_play.hpp"

namespace fall_line::sound {

struct RunSound {
  ge::skier::RideBed bed;
  Seat seat = Seat::chase;
  bool thunder = true;  // the storm's thunder heard
  ge::wind::ThunderPlayer heard;
  ge::wind::Storm storm;  // the map's, laid for `storm_level` at `storm_thunder`
  const ge::mountain::mapgen::Level* storm_level = nullptr;
  double storm_thunder = 0;
  ge::core::Rng rng = ge::core::make_rng(0x7a12u);  // a splash's bubbles (water/sound.hpp)
};

RunSound make_run_sound();

// The one-shots for this step's events.
void run_sound_step(RunSound& sound, ge::audio::Synth& synth, const sim::GameState& state);

// Steer the ride bed for this frame. `duck` scales the whole bed, 0..1 (1 on his skis, less under
// a card).
void run_sound_frame(RunSound& sound, ge::audio::Synth& synth, const sim::GameState& state,
                     float dt, float duck = 1);

// One of the menus' sounds (bank.hpp's kMenuFocus, kMenuPress, kMenuPage) on the effects bus,
// heard whatever seat the run has.
void play_menu(ge::audio::Synth& synth, std::string_view id);

// The run is paused or left: the bed torn down, so nothing holds its last note behind a card.
void run_sound_silence(RunSound& sound, ge::audio::Synth& synth);

}  // namespace fall_line::sound
