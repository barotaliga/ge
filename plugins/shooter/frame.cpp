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
  sv.time = sv.set.start_time + static_cast<double>(sv.frame) * sv.set.mintic;
  for (int i = 0; i < count; i++) run_packet(sv, packets[i]);
  const double frametime = sv.physics_started ? sv.set.mintic : sv.set.maxtic;
  sv.physics_started = true;
  sv.last_physics = sv.time;
  physics_frame(sv, frametime);
}

}  // namespace ge::shooter
