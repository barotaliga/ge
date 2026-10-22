// THE SNOWMOBILE AS A RIGID BODY: one step of the forces, the torques, Euler's equations, the
// hull's impulses and the move (sled.hpp's steps 1 to 6). `sled.cpp` calls it and applies the
// trunks, the map's edge and what throws the rider after.
//
// ge:summary The snowmobile's rigid body: suspension, grip, carve, roll, yaw, air, hull
// ge:keywords snowmobile body rigid-body ride_body probes suspension grip carve roll tip hull
// ge:keywords euler inertia derive_sled landing-pitch
// ge:skill snowmobile
// ge:test tests/snowmobile_scenarios_test.cpp
#pragma once

#include "snowmobile/sled.hpp"

namespace ge::snowmobile {

// What a body step reports: the hardest closing speed into the snow, m/s, and the length of the
// flight it landed from this step, s (0 when it did not land).
struct BodyReport {
  double impact = 0;
  double landed = 0;
};

// ONE STEP of the body under `in` (eased through their lags here), carrying `mass` kg in all of
// which `rider` kg is the rider standing on the boards.
BodyReport ride_body(SledState& s, const SledControls& in, double mass, double rider,
                     const SledWorld& world, double dt);

// The readouts derived from the body's state: heading, pitch, roll, speed, way.
void derive_sled(SledState& s);

}  // namespace ge::snowmobile
