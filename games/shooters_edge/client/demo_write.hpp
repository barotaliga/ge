// RECORDED GAMES WRITTEN IN CODE, for the tests and the labs that read and benchmark demos
// (demo.hpp, timedemo.hpp) without any recorded file: a scripted path of the player's eye
// through a map, written in any of the three formats with the messages a real recording carries
// around it (the level's start and precache lists, baselines, stats, sounds, effects, prints, a
// moving entity), so every reader is walked end to end.
//
// Coordinates go on the wire as eighths of a unit and angles as 1/256 of a turn (a view angle
// in a block or a command as an f32), so a script whose origins are whole eighths reads back
// exactly.
//
// ge:summary Recorded games written in code: a scripted eye path in every demo format
// ge:keywords demo-writer synthetic-demo fixture write_single_demo write_client_demo
// ge:keywords write_multiview_demo multiview
// ge:keywords demo-script scripted-path test-demo
// ge:skill shooter testing
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::client {

struct DemoScript {
  std::string map = "halls";                // written as maps/<map>.bsp
  std::vector<ge::shooter::Vec3f> origins;  // the player's origin a frame, units
  std::vector<ge::shooter::Vec3f> angles;   // the view a frame, degrees
  float frame_s = 1.0f / 72;                // the demo's clock between frames, s
  int health = 100;
  int items = 1 | 32 | 8192;  // the shotgun, the rocket launcher, the light armour
  int active_weapon = 32;
  int rockets = 25;
};

// The single-player demo: the music track line, a signon block, a block a frame, a disconnect.
std::vector<uint8_t> write_single_demo(const DemoScript& script);
// The client recording: the signon packets, then a command and a packet a frame; `cmd_size` 24
// or 28 (a client that pads its command struct).
std::vector<uint8_t> write_client_demo(const DemoScript& script, int cmd_size = 24);
// A scripted walk round `map`'s floor: `frames` frames along a closed loop of `radius` units
// about `centre`, turning to look along it, a full turn of the view's pitch's sway.
// The multiview recording: the signon to everyone, two players' stats records, then a frame
// every `frame_s` (whole milliseconds): the scripted player in slot 0 and a second one standing
// in slot 3, a rocket in the entity list, a hidden record, a print and stats for slot 3 alone.
std::vector<uint8_t> write_multiview_demo(const DemoScript& script);
DemoScript loop_script(const std::string& map, ge::shooter::Vec3f centre, float radius, int frames);

}  // namespace shooters_edge::client
