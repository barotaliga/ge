// The player's view frame by frame (present.hpp).
//
// ge:summary The player's view frame by frame: the first-person eyes and the missile markers
// ge:keywords present frame_view draw_markers on_screen blasts
// ge:skill camera ui shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/present.hpp"

#include <algorithm>

#include "brush/map.hpp"
#include "camera/to_view.hpp"
#include "core/libm/libm.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace libm = ge::core::libm;

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

bool on_screen(const ge::render::FrameView& view, ge::camera::Vec3 p, int w, int h, float& sx,
               float& sy) {
  // The heading is clockwise from +z (seen from above), so the right is (cos, 0, -sin).
  const double sh = libm::sin(view.heading), ch = libm::cos(view.heading);
  const double sp = libm::sin(view.pitch), cp = libm::cos(view.pitch);
  const double d[3] = {p.x - view.eye[0], p.y - view.eye[1], p.z - view.eye[2]};
  const double f = d[0] * sh * cp + d[1] * sp + d[2] * ch * cp;
  if (f < 0.1) return false;
  const double r = d[0] * ch - d[2] * sh;
  const double u = -d[0] * sh * sp + d[1] * cp - d[2] * ch * sp;
  const double t = libm::tan(view.fov_y / 2);
  const double aspect = static_cast<double>(w) / std::max(1, h);
  sx = static_cast<float>(w / 2.0 * (1 + r / f / (t * aspect)));
  sy = static_cast<float>(h / 2.0 * (1 - u / f / t));
  return true;
}

void draw_markers(ge::ui::DrawList& dl, const ViewState& v, const ge::render::FrameView& view,
                  int w, int h, double t) {
  for (const Missile& m : v.missiles) {
    float x = 0, y = 0;
    if (!on_screen(view, engine_point(m.origin), w, h, x, y)) continue;
    const float r = m.rocket ? 5.0f : 2.5f;
    dl.rect({x - r, y - r, 2 * r, 2 * r}, ge::ui::Color{1.0f, 0.75f, 0.3f, 1.0f}, r);
  }
  for (const Blast& b : v.blasts) {
    float x = 0, y = 0;
    if (!on_screen(view, b.at, w, h, x, y)) continue;
    const float age = static_cast<float>((t - b.t) / kBlastS);
    const float r = 10 + 60 * age;
    dl.rect({x - r, y - r, 2 * r, 2 * r}, ge::ui::Color{1.0f, 0.5f, 0.15f, 0.6f * (1 - age)}, r);
  }
}

}  // namespace shooters_edge::client
