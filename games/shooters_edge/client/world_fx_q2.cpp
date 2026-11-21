// QUAKE II'S OWN EFFECTS IN THE WORLD (client/world_fx.hpp): the temp entities that come as named
// effects (`sq_*`: arrivals, an item's return, sparks off a weld or a tunnel, steam and smoke, the
// blue hyperblaster's hit, the widow's splash, the flashlight), the trails along a line (bubbles,
// a force wall) and the marks a flying entity's effect bits leave (a flag's or the tag's column,
// the ion ripper's specks, the trap's glow, the tracker's strands and dark light, the spinning
// lights, each one's light), thrown by world_fx.cpp's effect loop.
//
// ge:summary Quake II's named effects, line trails and effect-bit marks thrown into the world
// ge:keywords q2-effects temp-entity te_bubbletrail te_forcewall te_welding_sparks te_steam
// ge:keywords te_flashlight ef_trap ef_tracker ef_trackertrail ef_spinninglights
// ge:keywords ef_flag1 ef_flag2 ef_ionripper ef_tagtrail ef_plasma q2_mark
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#include <algorithm>
#include <cmath>
#include <string_view>

#include "brush/map.hpp"

#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/client/q2_look.hpp"
#include "shooters_edge/client/world_fx.hpp"

namespace shooters_edge::client {

namespace fx = shooters_edge::effects;

namespace {
constexpr double kSqBeamOutS = 2.1;  // a widow beam-out's life
}  // namespace

void q2_named_fx(FxState& s, const Effect& e, const double p[3], bool dust, double t,
                 FxFrame& out) {
  const std::string_view n = e.name;
  if (n == "sq_widow_beam") {  // kept for 2.1 s, one per id (a later one takes its place)
    std::erase_if(s.beam_outs, [&](const FxState::BeamOut& b) { return b.id == e.entity; });
    if (e.entity == 0 || s.beam_outs.size() >= 32) return;
    FxState::BeamOut b;
    b.id = e.entity;
    for (int k = 0; k < 3; ++k) b.at[k] = p[k];
    b.born = t;
    s.beam_outs.push_back(b);
    return;
  }
  if (n == "sq_power_splash") {
    fx::q2_power_splash(p, e.way.x, s.seed++, s.palette, &out.births);
    return;
  }
  if (n == "sq_teleport" && !dust)
    fx::q2_teleport_burst(p, s.seed++, s.palette, &out.births);
  else if (n == "sq_item_back")
    fx::q2_item_back(p, s.seed++, s.palette, &out.births);
  else if (n == "sq_fountain")
    fx::q2_fountain(p, s.seed++, s.palette, &out.births);
  else if (n == "sq_laser_sparks" || n == "sq_splash") {
    const double off_face[3] = {e.way.x, e.way.y, e.way.z};
    const bool splash = n == "sq_splash";
    fx::q2_specks(p, off_face, e.count, e.colour, splash ? 32 : 8, splash, s.seed++, s.palette,
                  &out.births);
  } else if (n == "sq_welding" || n == "sq_tunnel_sparks") {
    const double off_face[3] = {e.way.x, e.way.y, e.way.z};
    const bool rising = n == "sq_tunnel_sparks";
    fx::q2_specks(p, off_face, e.count, e.colour, 8, false, s.seed++, s.palette, &out.births,
                  rising ? -80 : 80);
    if (!rising)  // the welder's light, yellow-white, for the frame it is thrown in
      out.lights.push_back(fx::q2_point_light(p, 100 + double(s.seed++ % 75), 1, 1, 0.3));
  } else if (n == "sq_steam" || n == "sq_smoke") {
    const bool smoke = n == "sq_smoke";
    const double jet[3] = {smoke ? 0.0 : e.way.x, smoke ? 0.0 : e.way.y, smoke ? 20.0 : e.way.z};
    fx::q2_steam(p, jet, smoke ? 20 : e.count, smoke ? 0 : e.colour, smoke ? 0 : 40, s.seed++,
                 s.palette, &out.births);
  } else if (n == "sq_blue_hit") {
    const double way[3] = {e.way.x, e.way.y, e.way.z};
    fx::q2_blue_hit(p, way, s.seed++, s.palette, &out.births);
  } else if (n == "sq_widow_splash") {
    fx::q2_widow_splash(p, s.seed++, s.palette, &out.births);
  } else if (n == "sq_flashlight") {
    out.lights.push_back(fx::q2_point_light(p, 400, 1, 1, 1));
  } else if (n == "sq_logout")
    fx::q2_player_flash(p, fx::kSqLogout, s.seed++, s.palette, &out.births);
  else if ((n == "sq_login" || n == "sq_respawn") && !dust)
    fx::q2_player_flash(p, n == "sq_login" ? fx::kSqLogin : fx::kSqRespawn, s.seed++, s.palette,
                        &out.births);
  // The flash's light, for the one frame it is thrown in.
  if (n == "sq_login" || n == "sq_logout" || n == "sq_respawn") {
    const fx::Q2Arrival kind = n == "sq_login"    ? fx::kSqLogin
                               : n == "sq_logout" ? fx::kSqLogout
                                                  : fx::kSqRespawn;
    out.lights.push_back(fx::q2_arrival_light(p, kind, s.seed++));
  }
}

void q2_sustains(FxState& s, double t, FxFrame& out) {
  std::erase_if(s.beam_outs, [&](const FxState::BeamOut& b) { return t - b.born > kSqBeamOutS; });
  for (const FxState::BeamOut& b : s.beam_outs)
    fx::q2_beam_out(b.at, std::clamp((t - b.born) / kSqBeamOutS, 0.0, 1.0), s.seed++, s.palette,
                    &out.births);
}

bool q2_named_trail(FxState& s, const Effect& e, const double from[3], const double to[3],
                    FxFrame& out) {
  if (e.name == "sq_bubbles" || e.name == "sq_bubbles2") {
    fx::q2_bubbles(from, to, e.name == "sq_bubbles2", s.seed++, s.palette, &out.births);
    return true;
  }
  if (e.name == "sq_force_wall") {
    fx::q2_force_wall(from, to, e.colour, s.seed++, s.palette, &out.births);
    return true;
  }
  return false;
}

void q2_mark_fx(FxState& s, const Missile& m, bool moved, const double from[3], const double to[3],
                double t, FxFrame& out) {
  using K = Q2Look::Missile;
  const auto k = static_cast<K>(m.q2_mark);
  // Spinning lights: a red light 64 units ahead along the spin, whatever else it carries.
  if (m.q2_spin) {
    const double yaw = (t * kSqSpinDegPerS + m.yaw) * 3.14159265358979323846 / 180;
    const double ahead[3] = {to[0] + 64 * ge::brush::kUnitM * std::cos(yaw), to[1],
                             to[2] + 64 * ge::brush::kUnitM * std::sin(yaw)};
    out.lights.push_back(fx::q2_point_light(ahead, 100, 1, 0, 0));
  }
  if (k == K::none) return;
  if (k == K::trap) {  // its glow 32 units over it, a light flickering each frame
    const double up[3] = {to[0], to[1] + 32 * ge::brush::kUnitM, to[2]};
    fx::q2_trap(up, s.seed++, s.palette, &out.births);
    out.lights.push_back(fx::q2_point_light(up, 100 + double(s.seed++ % 100), 1, 0.8, 0.1));
    return;
  }
  if (k == K::tracker_dark) {  // no specks; a darkening light whose reach pulses
    const double reach = 50 + 500 * (std::sin(t * 1000 / 500) + 1);
    out.lights.push_back(fx::q2_point_light(to, reach, -1, -1, -1));
    return;
  }
  if (k == K::tracker_shell) {  // a black shell round it, a darkening light
    fx::q2_tracker_shell(to, 40, s.seed++, s.palette, &out.births);
    out.lights.push_back(fx::q2_point_light(to, 155, -1, -1, -1));
    return;
  }
  if (k == K::tracker) {  // two black strands and a darkening light
    if (moved) fx::q2_tracker_trail(from, to, s.seed++, s.palette, &out.births);
    out.lights.push_back(fx::q2_point_light(to, 200, -1, -1, -1));
    return;
  }
  if (moved) {
    if (k == K::flag1 || k == K::flag2 || k == K::tag)
      fx::q2_column_trail(from, to,
                          k == K::flag1   ? 242
                          : k == K::flag2 ? 115
                                          : 220,
                          s.seed++, s.palette, &out.births);
    else if (k == K::ion) {
      int side = 1;
      fx::q2_ion_trail(from, to, side, s.seed++, s.palette, &out.births);
    }
  }
  const double r = k == K::ion ? 100 : k == K::blue ? 200 : k == K::plasma ? 130 : 225;
  const double red = k == K::flag2 ? 0.1 : k == K::blue ? 0 : 1;
  const double green = k == K::flag1 || k == K::flag2 ? 0.1
                       : k == K::tag                  ? 1
                       : k == K::blue                 ? 0
                                                      : 0.5;
  const double blue = k == K::flag2 || k == K::blue ? 1
                      : k == K::flag1               ? 0.1
                      : k == K::tag                 ? 0
                                                    : 0.5;
  out.lights.push_back(fx::q2_point_light(to, r, red, green, blue));
}

}  // namespace shooters_edge::client
