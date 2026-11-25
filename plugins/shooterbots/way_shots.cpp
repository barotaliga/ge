// The way through's shot buttons (way_shots.hpp).
//
// ge:summary The way through's shot buttons: shot at their face, from a node he reaches
// ge:keywords way_face_shot_buttons way_reach_shot_buttons shot-button button-face
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooterbots/way_shots.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "shooter/trace.hpp"

namespace ge::shooterbots {

namespace {

namespace sh = ge::shooter;
namespace sb = ge::shooterbots;

constexpr float kWayShotAcross = 900;  // a shot button is shot from this near, units
constexpr double kWayRepick_s = 10;    // an unreached one's node looked for again after this, s

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// The edict the mod made of inline model `model`, or -1.
int edict_of(const ge::shooterbots::BotView& s, const char* model) {
  for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free || d.modelindex <= 0 || static_cast<size_t>(d.modelindex) >= s.models.size())
      continue;
    if (s.models[static_cast<size_t>(d.modelindex)] == model) return e;
  }
  return -1;
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

void way_face_shot_buttons(Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
                           const std::vector<EntityBlock>& blocks) {
  const NavGraph& g = nav.graph;
  std::vector<uint8_t> on_bridge(g.nodes.size(), 0);
  for (const NavBridge& br : nav.bridges)
    if (!br.floor && !br.touch)
      for (const int32_t n : br.nodes) on_bridge[static_cast<size_t>(n)] = 1;
  for (ButtonSpot& bs : f.button_spots) {
    if (!bs.shoot || bs.node < 0 || !bs.model.empty()) continue;
    // Its face, from the angle its press moves it along (up, down, or level at a right angle;
    // another angle keeps the node it has).
    sh::Vec3f face{};
    for (const EntityBlock& b : blocks) {
      const char* cn = block_value(b, "classname");
      const char* model = block_value(b, "model");
      const char* an = block_value(b, "angle");
      if (!cn || !model || std::strcmp(cn, "func_button") != 0 || edict_of(s, model) != bs.ent)
        continue;
      const int a = an ? std::atoi(an) : 0;
      face = a == -1    ? sh::Vec3f{0, 0, -1}
             : a == -2  ? sh::Vec3f{0, 0, 1}
             : a == 0   ? sh::Vec3f{-1, 0, 0}
             : a == 90  ? sh::Vec3f{0, -1, 0}
             : a == 180 ? sh::Vec3f{1, 0, 0}
             : a == 270 ? sh::Vec3f{0, 1, 0}
                        : sh::Vec3f{};
    }
    bs.face = face;
    const bool known = dot(face, face) != 0;
    // A node behind it, or on a bridge still in, is looked for again in the plan among the
    // nodes he reaches (way_reach_shot_buttons); any other is kept.
    const sh::Vec3f was = g.nodes[static_cast<size_t>(bs.node)].origin;
    const bool bridged = on_bridge[static_cast<size_t>(bs.node)] != 0;
    bs.refind = bridged || (known && dot(was - bs.at, face) <= 0);
    if (!bridged) continue;
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

void way_reach_shot_buttons(WayThrough& w, Fighter& f, const NavGraph& g,
                            const ge::shooterbots::BotView& s, double t) {
  for (WayStep& st : w.steps) {
    if (st.kind != WayKind::button || st.done || st.node < 0 || w.search.reached(st.node) ||
        t < st.repick_at)
      continue;
    ButtonSpot& bs = f.button_spots[static_cast<size_t>(st.button)];
    // (One at its face from the start is a way to it the plan has yet to open: e2m2's pair,
    // given another node, cost 11 of its kills.)
    if (!bs.shoot || !bs.refind || dot(bs.face, bs.face) == 0) continue;
    st.repick_at = t + kWayRepick_s;
    int32_t best = -1;
    float best_d = kWayShotAcross;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      const float dd = dist(o, bs.at);
      if (dd >= best_d || g.nodes[n].mover >= 0 || !at_face(o, bs.at, bs.face) ||
          !w.search.reached(static_cast<int32_t>(n)) || !sees(s, o, bs))
        continue;
      best = static_cast<int32_t>(n);
      best_d = dd;
    }
    if (best >= 0) st.node = bs.node = best;
  }
}

}  // namespace ge::shooterbots
