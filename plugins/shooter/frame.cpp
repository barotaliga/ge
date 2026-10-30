// The server's frame (frame.hpp states the order).
//
// ge:summary The server's frame: the step's time, the packets in order, then physics
// ge:keywords server_frame time packets physics
// ge:skill shooter
// ge:test tests/shooter_client_test.cpp
#include "shooter/frame.hpp"

#include "shooter/physics.hpp"

namespace ge::shooter {

void server_frame(Server& sv, const Packet* packets, int count) {
  sv.sound_count = 0;
  sv.frame++;
  const double end = sv.base_time + static_cast<double>(sv.frame - sv.base_frame) * sv.set.mintic;
  if (sv.set.single_player_clock)
    sv.time = end - sv.set.mintic;
  else
    sv.time = end;
  for (int i = 0; i < count; i++) run_packet(sv, packets[i]);
  const double frametime =
      sv.physics_started || sv.set.single_player_clock ? sv.set.mintic : sv.set.maxtic;
  sv.physics_started = true;
  sv.last_physics = sv.time;
  physics_frame(sv, frametime);
  sv.time = end;
}

void settle_frames(Server& sv, int frames, double dt) {
  for (int i = 0; i < frames; i++) {
    sv.sound_count = 0;
    sv.frame++;
    sv.physics_started = true;
    sv.last_physics = sv.time;
    physics_frame(sv, dt);
    sv.time += dt;
  }
  sv.base_time = sv.time;
  sv.base_frame = sv.frame;
}

}  // namespace ge::shooter
