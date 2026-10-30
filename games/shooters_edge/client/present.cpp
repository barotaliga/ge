// The player's view frame by frame (present.hpp).
//
// ge:summary The player's view frame by frame: the first-person eyes, the missiles and blasts
// ge:keywords present frame_view missiles blasts
// ge:skill camera ui shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/present.hpp"

#include <algorithm>

#include "brush/map.hpp"
#include "camera/to_view.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

ge::render::FrameView frame_view(const Listen& l, ViewState& v, int frames, double t, double dt) {
  // The mod's explosions since the last look (a rocket at point blank goes off in the frame that
  // fired it, so it is never seen in flight).
  if (frames > 0) missiles_of(l, v.missiles);
  if (l.explosions.size() < v.explosions_seen) v.explosions_seen = 0;  // a new map
  for (; v.explosions_seen < l.explosions.size(); v.explosions_seen++)
    v.blasts.push_back({engine_point(l.explosions[v.explosions_seen]), t});
  std::erase_if(v.blasts, [&](const Blast& b) { return t - b.t > kBlastS; });

  ge::camera::Subject s = subject_of(l, v.row);
  if (l.forced) ge::camera::add_moment(s, ge::camera::Moment::teleport, 0);
  if (s.grounded && !v.was_grounded)
    ge::camera::add_moment(s, ge::camera::Moment::landing,
                           -static_cast<double>(v.fall_speed) * ge::brush::kUnitM);
  v.was_grounded = s.grounded;
  v.fall_speed = player_state(l).velocity.z;
  ge::render::FrameView view;
  view.time_s = t;
  ge::camera::to_frame_view(ge::camera::frame_first_person(v.row, s, v.eyes, dt, 1.0), view);
  return view;
}

}  // namespace shooters_edge::client
