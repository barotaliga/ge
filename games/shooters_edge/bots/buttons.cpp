// The map's buttons (buttons.hpp): read and linked once, and a novice's pokes.
//
// ge:summary The map's buttons linked to doors and items; a novice's random shots and touches
// ge:keywords buttons_init button_poke button_poke_cmd button_node door-keeps trigger_relay
// ge:keywords relayed-button button_fires
// ge:skill shooter bots
// ge:test tests/shooters_edge_footwork_test.cpp
#include "shooters_edge/bots/buttons.hpp"

#include "core/prng.hpp"

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

float draw(Fighter& f) { return static_cast<float>(ge::core::next(f.rng)); }

sh::Vec3f middle(const server::WorldEntity& e) {
  return {(e.absmin.x + e.absmax.x) * 0.5f, (e.absmin.y + e.absmax.y) * 0.5f,
          (e.absmin.z + e.absmax.z) * 0.5f};
}

// The line from `eye` meets nothing before the button.
bool sees(const server::RulesCommon& s, sh::Vec3f eye, const ButtonSpot& b) {
  const sh::HullTrace tr = sh::trace_move(s.sv, eye, {}, {}, b.at, sh::kTraceNoMonsters, -1);
  return tr.fraction >= 1 || tr.ent == b.ent;
}

// The node a button is pressed (or shot) from: the nearest within reach whose line meets the
// button itself, so a node behind the wall the button is set in is never chosen.
int32_t button_node(const sh::NavGraph& g, const server::RulesCommon& s, const ButtonSpot& b,
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

std::vector<std::string> button_fires(const std::vector<pg::EntityBlock>& blocks,
                                      const char* target) {
  std::vector<std::string> names;
  if (!target || !target[0]) return names;
  names.emplace_back(target);
  for (size_t from = 0, depth = 0; from < names.size() && depth < kRelayDepth; depth++) {
    const size_t to = names.size();
    for (; from < to; from++)
      for (const pg::EntityBlock& b : blocks) {
        const char* cn = pg::block_value(b, "classname");
        const char* tn = pg::block_value(b, "targetname");
        const char* next = pg::block_value(b, "target");
        if (!cn || !tn || !next || !next[0] || names[from] != tn) continue;
        if (std::strcmp(cn, "trigger_relay") != 0 && std::strcmp(cn, "trigger_once") != 0 &&
            std::strcmp(cn, "trigger_multiple") != 0)
          continue;
        bool seen = false;
        for (const std::string& n : names) seen |= n == next;
        if (!seen) names.emplace_back(next);
      }
  }
  return names;
}

void buttons_init(Fighter& f, const sh::NavGraph& g, const server::RulesCommon& s,
                  const char* entities) {
  f.button_spots.clear();
  f.poke = -1;
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!entities || !pg::parse_entity_text(entities, blocks, error)) return;
  const auto entity_on = [&](const char* model) {
    for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
      const server::WorldEntity& d = s.world.entities[static_cast<size_t>(e)];
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
    bs.at = middle(s.world.entities[static_cast<size_t>(bs.ent)]);
    const char* health = pg::block_value(b, "health");
    bs.shoot = health && std::atof(health) > 0;
    bs.node = button_node(g, s, bs, bs.shoot ? 900.f : 256.f);
    const std::vector<std::string> fired = button_fires(blocks, target);
    for (const pg::EntityBlock& d : blocks) {
      const char* dc = pg::block_value(d, "classname");
      const char* dn = pg::block_value(d, "targetname");
      const char* dm = pg::block_value(d, "model");
      if (!dc || std::strcmp(dc, "func_door") != 0 || !dn || !dm) continue;
      bool opened = false;
      for (const std::string& n : fired) opened |= n == dn;
      if (!opened) continue;
      const int door = entity_on(dm);
      if (door < 0) continue;
      bs.doors.push_back(door);
      const server::WorldEntity& de = s.world.entities[static_cast<size_t>(door)];
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

void exits_init(Fighter& f, const sh::NavGraph& g, const server::RulesCommon& s,
                const char* entities) {
  f.exits.clear();
  f.locked.clear();
  f.locked_rest.clear();
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!entities || !pg::parse_entity_text(entities, blocks, error)) return;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* model = pg::block_value(b, "model");
    // A door that only something else opens: one with a name (not one that starts open), or a
    // secret door. A touch does not open it, so a route through it waits there for ever.
    if (cn && model && model[0] == '*') {
      const char* name = pg::block_value(b, "targetname");
      const char* sf = pg::block_value(b, "spawnflags");
      const bool start_open = sf && (std::atoi(sf) & 1) != 0;
      if ((std::strcmp(cn, "func_door") == 0 && name && !start_open) ||
          std::strcmp(cn, "func_door_secret") == 0) {
        for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
          const server::WorldEntity& d = s.world.entities[static_cast<size_t>(e)];
          if (!d.free && d.modelindex > 0 && static_cast<size_t>(d.modelindex) < s.models.size() &&
              s.models[static_cast<size_t>(d.modelindex)] == model) {
            f.locked.push_back(e);
            f.locked_rest.push_back(d.origin);
            break;
          }
        }
      }
    }
    if (!cn || std::strcmp(cn, "trigger_changelevel") != 0 || !model || model[0] != '*') continue;
    // The trigger's brush model by its name in the model list (a trigger draws none, so no
    // entity carries the index): its bounds are the box.
    size_t mi = 0;
    for (size_t i = 1; i < s.models.size() && i < s.sv.models.size(); i++)
      if (s.models[i] == model) mi = i;
    if (mi == 0) continue;
    ExitSpot x;
    x.lo = s.sv.models[mi].mins;
    x.hi = s.sv.models[mi].maxs;
    // The box a standing player at a node fills: 16 each way across, 24 below, 32 above.
    const auto gap = [&](sh::Vec3f o) {
      const float dx = std::fmax(std::fmax(x.lo.x - (o.x + 16), (o.x - 16) - x.hi.x), 0.f);
      const float dy = std::fmax(std::fmax(x.lo.y - (o.y + 16), (o.y - 16) - x.hi.y), 0.f);
      const float dz = std::fmax(std::fmax(x.lo.z - (o.z + 32), (o.z - 24) - x.hi.z), 0.f);
      return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    float best = 1e30f;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      const float d = gap(g.nodes[n].origin);
      if (d <= kExitMargin) {
        x.inside.push_back(static_cast<int32_t>(n));
      } else if (d >= kExitClear && d <= kExitStandFar && d < best) {
        best = d;
        x.stand = static_cast<int32_t>(n);
      }
    }
    f.exits.push_back(std::move(x));
  }
}

bool button_poke(Fighter& f, const server::RulesCommon& s, int me, double t) {
  if (f.sense != MapSense::novice || f.button_spots.empty()) return false;
  if (f.poke >= 0 && t < f.poke_until) return true;
  f.poke = -1;
  if (t < f.next_poke) return false;
  f.next_poke = t + kPokeEvery_s;
  if (draw(f) >= kPokeChance) return false;
  const sh::Vec3f self = s.world.entities[static_cast<size_t>(me)].origin;
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

void button_poke_cmd(Fighter& f, const server::RulesCommon& s, int me, double t, sh::MoveCmd& cmd) {
  if (f.poke < 0 || t >= f.poke_until || f.bot.aiming) return;
  const ButtonSpot& b = f.button_spots[static_cast<size_t>(f.poke)];
  const sh::Vec3f self = s.world.entities[static_cast<size_t>(me)].origin;
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
