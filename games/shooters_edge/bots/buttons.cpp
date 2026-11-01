// The map's buttons (buttons.hpp): read and linked once, and a novice's pokes.
//
// ge:summary The map's buttons linked to doors and items; a novice's random shots and touches
// ge:keywords buttons_init button_poke button_poke_cmd button_node door-keeps
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/buttons.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "progs/entity_text.hpp"
#include "shooter/trace.hpp"

namespace shooters_edge::bots {

namespace {

namespace sh = ge::shooter;
namespace pg = ge::progs;

constexpr double kPokeEvery_s = 12;  // a novice's whim comes this often
constexpr float kPokeChance = 0.4f;  // and takes him this often
constexpr double kPokeFor_s = 8;     // he gives a button this long
constexpr float kPokeSight = 700;    // a button this near and in sight tempts him, units

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

float draw(Fighter& f) {
  f.rng = f.rng * 1664525u + 1013904223u;
  return static_cast<float>(f.rng >> 8) / 16777216.f;
}

sh::Vec3f middle(const sh::Edict& e) {
  return {(e.absmin.x + e.absmax.x) * 0.5f, (e.absmin.y + e.absmax.y) * 0.5f,
          (e.absmin.z + e.absmax.z) * 0.5f};
}

// The line from `eye` meets nothing before the button.
bool sees(const server::QcServer& s, sh::Vec3f eye, const ButtonSpot& b) {
  const sh::HullTrace tr = sh::trace_move(s.sv, eye, {}, {}, b.at, sh::kTraceNoMonsters, -1);
  return tr.fraction >= 1 || tr.ent == b.ent;
}

// The node a button is pressed (or shot) from: the nearest within reach whose line meets the
// button itself, so a node behind the wall the button is set in is never chosen.
int32_t button_node(const sh::NavGraph& g, const server::QcServer& s, const ButtonSpot& b,
                    float reach) {
  int32_t best = -1;
  float best_d = reach;
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sh::Vec3f o = g.nodes[n].origin;
    const float d = dist(o, b.at);
    if (d >= best_d || !sees(s, {o.x, o.y, o.z + 22}, b)) continue;
    best = static_cast<int32_t>(n);
    best_d = d;
  }
  return best >= 0 ? best : sh::nav_nearest(g, b.at);
}

// Within `r` of the box from `lo` to `hi`.
bool near_box(sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi, float r) {
  return p.x >= lo.x - r && p.x <= hi.x + r && p.y >= lo.y - r && p.y <= hi.y + r &&
         p.z >= lo.z - r && p.z <= hi.z + r;
}

}  // namespace

void buttons_init(Fighter& f, const sh::NavGraph& g, const server::QcServer& s,
                  const char* entities) {
  f.button_spots.clear();
  f.poke = -1;
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!entities || !pg::parse_entity_text(entities, blocks, error)) return;
  const auto entity_on = [&](const char* model) {
    for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
      if (!d.free && d.modelindex > 0 && static_cast<size_t>(d.modelindex) < s.models.size() &&
          s.models[static_cast<size_t>(d.modelindex)] == model)
        return e;
    }
    return -1;
  };
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* target = pg::block_value(b, "target");
    const char* model = pg::block_value(b, "model");
    if (!cn || std::strcmp(cn, "func_button") != 0 || !model) continue;
    ButtonSpot bs;
    bs.ent = entity_on(model);
    if (bs.ent < 0) continue;
    bs.at = middle(s.sv.edicts[static_cast<size_t>(bs.ent)]);
    const char* health = pg::block_value(b, "health");
    bs.shoot = health && std::atof(health) > 0;
    bs.node = button_node(g, s, bs, bs.shoot ? 900.f : 256.f);
    for (const pg::EntityBlock& d : blocks) {
      const char* dc = pg::block_value(d, "classname");
      const char* dn = pg::block_value(d, "targetname");
      const char* dm = pg::block_value(d, "model");
      if (!target || !dc || std::strcmp(dc, "func_door") != 0 || !dn ||
          std::strcmp(dn, target) != 0 || !dm)
        continue;
      const int door = entity_on(dm);
      if (door < 0) continue;
      bs.doors.push_back(door);
      const sh::Edict& de = s.sv.edicts[static_cast<size_t>(door)];
      for (size_t i = 0; i < f.items.size(); i++) {
        ItemSpot& it = f.items[i];
        if (!near_box(it.at, de.absmin, de.absmax, kDoorKeeps)) continue;
        bool listed = false;
        for (int32_t o : bs.opens) listed |= o == static_cast<int32_t>(i);
        if (!listed) bs.opens.push_back(static_cast<int32_t>(i));
        if (f.sense == MapSense::novice) continue;  // he knows no button
        it.door_ent = door;
        it.door_rest = de.origin;
        it.button_at = bs.at;
        it.button_shoot = bs.shoot;
        it.button_ent = bs.ent;
        it.button_node = bs.node;
      }
    }
    f.button_spots.push_back(std::move(bs));
  }
}

bool button_poke(Fighter& f, const server::QcServer& s, int me, double t) {
  if (f.sense != MapSense::novice || f.button_spots.empty()) return false;
  if (f.poke >= 0 && t < f.poke_until) return true;
  f.poke = -1;
  if (t < f.next_poke) return false;
  f.next_poke = t + kPokeEvery_s;
  if (draw(f) >= kPokeChance) return false;
  const sh::Vec3f self = s.sv.edicts[static_cast<size_t>(me)].origin;
  const sh::Vec3f eye{self.x, self.y, self.z + 22};
  float best = kPokeSight;
  for (size_t k = 0; k < f.button_spots.size(); k++) {
    const ButtonSpot& b = f.button_spots[k];
    const float d = dist(b.at, eye);
    if (d >= best || b.node < 0 || !sees(s, eye, b)) continue;
    if (f.reach_at >= 0 && !f.reach.reached(b.node)) continue;
    best = d;
    f.poke = static_cast<int32_t>(k);
  }
  if (f.poke < 0) return false;
  f.poke_until = t + kPokeFor_s;
  f.stats.pokes++;
  return true;
}

void button_poke_cmd(Fighter& f, const server::QcServer& s, int me, double t, sh::MoveCmd& cmd) {
  if (f.poke < 0 || t >= f.poke_until || f.bot.aiming) return;
  const ButtonSpot& b = f.button_spots[static_cast<size_t>(f.poke)];
  const sh::Vec3f self = s.sv.edicts[static_cast<size_t>(me)].origin;
  const sh::Vec3f eye{self.x, self.y, self.z + 22};
  if (!sees(s, eye, b)) return;
  const sh::Vec3f d = b.at - eye;
  const double flat = std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y);
  if (flat > 900) return;
  cmd.angles.y = static_cast<float>(ge::core::libm::atan2(d.y, d.x) * (180.0 / ge::core::kPi));
  cmd.angles.x = static_cast<float>(-ge::core::libm::atan2(d.z, flat) * (180.0 / ge::core::kPi));
  if (flat < 200) {
    cmd.forward = 320;  // into it: a touch presses it
    cmd.side = 0;
  }
  // He fires at any button, whatever opens it (a few shots a second, as a trigger finger does).
  if (f.stats.commands % 8 == 0)
    cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
}

}  // namespace shooters_edge::bots
