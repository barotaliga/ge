// The way through's shot buttons on a bridge (way_shots.hpp).
//
// ge:summary A shot button whose node is on a bridge still in: shot at its face from off it
// ge:keywords way_face_shot_buttons shot-button button-face bridge
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooterbots/way_shots.hpp"

#include <cmath>
#include <cstdint>

#include "shooter/trace.hpp"

namespace ge::shooterbots {

namespace {

namespace sh = ge::shooter;
namespace sb = ge::shooterbots;

constexpr float kWayShotAcross = 900;  // a shot button is shot from this near, units

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// Whether `o` is at the face `face` of a button whose middle is `at`: out from it, no more than
// 60 degrees off straight out (a node above or beside it sees its edge: r1m4's ledge over it was
// as far out of reach as the one behind).
bool at_face(sh::Vec3f o, sh::Vec3f at, sh::Vec3f face) {
  const sh::Vec3f v = o - at;
  const float out = dot(v, face);
  return out > 0 && out * out >= 0.25f * dot(v, v);
}

// Whether a player's eye at node origin `o` sees button `bs`.
bool sees(const ge::shooterbots::BotView& s, sh::Vec3f o, const ButtonSpot& bs) {
  const sh::HullTrace tr =
      sh::trace_move(s.sv, {o.x, o.y, o.z + 22}, {}, {}, bs.at, sh::kTraceNoMonsters, -1);
  return tr.fraction >= 1 || tr.ent == bs.ent;
}

}  // namespace

void way_face_shot_buttons(Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s) {
  const NavGraph& g = nav.graph;
  std::vector<uint8_t> on_bridge(g.nodes.size(), 0);
  for (const NavBridge& br : nav.bridges)
    if (!br.floor && !br.touch)
      for (const int32_t n : br.nodes) on_bridge[static_cast<size_t>(n)] = 1;
  for (ButtonSpot& bs : f.button_spots) {
    if (!bs.shoot || bs.node < 0 || !bs.model.empty()) continue;
    // Its face: the other side from the way a press pushes it in (zero for one pressed up or
    // down, whose node is then any that sees it).
    const sh::Vec3f face = bs.push * -1.0f;
    const bool known = dot(face, face) != 0;
    // A node off any bridge still in is kept (one he does not reach is sought among those he
    // does as the plan goes: way_shot_sides, `shotplace`).
    if (on_bridge[static_cast<size_t>(bs.node)] == 0) continue;
    int32_t best = -1;
    float best_d = kWayShotAcross;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      const float dd = dist(o, bs.at);
      if (dd >= best_d || on_bridge[n] != 0 || g.nodes[n].mover >= 0) continue;
      if ((known && !at_face(o, bs.at, face)) || !sees(s, o, bs)) continue;
      best = static_cast<int32_t>(n);
      best_d = dd;
    }
    if (best >= 0) bs.node = best;
  }
}

}  // namespace ge::shooterbots
