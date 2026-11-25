// The map's buttons (buttons.hpp): read and linked once, and a novice's pokes.
//
// ge:summary The map's buttons linked to doors and items; a novice's random shots and touches
// ge:keywords buttons_init button_poke button_poke_cmd button_node door-keeps trigger_relay
// ge:keywords relayed-button button_fires
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_footwork_test.cpp
#include "shooterbots/buttons.hpp"
#include "shooterbots/knowledge.hpp"

#include "core/prng.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/trace.hpp"
#include "shooterbots/aim.hpp"
#include "shooterbots/entities.hpp"

namespace ge::shooterbots {

namespace {

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;

constexpr double kPokeEvery_s = 12;      // a novice's whim comes this often
constexpr float kPokeChance = 0.4f;      // and takes him this often
constexpr double kPokeFor_s = 8;         // he gives a button this long
constexpr double kPokeLookHold_s = 0.6;  // (`pokelook`) the look held this long out of sight
constexpr float kPokeOn = 3;             // the view this near the button (degrees): a shot
constexpr float kPokeSight = 700;        // a button this near and in sight tempts him, units

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

float draw(Fighter& f) { return static_cast<float>(ge::core::next(f.rng)); }

sh::Vec3f middle(const ge::shooter::WorldEntity& e) {
  return {(e.absmin.x + e.absmax.x) * 0.5f, (e.absmin.y + e.absmax.y) * 0.5f,
          (e.absmin.z + e.absmax.z) * 0.5f};
}

// The line from `eye` meets nothing before the button.
bool sees(const ge::shooterbots::BotView& s, sh::Vec3f eye, const ButtonSpot& b) {
  const sh::HullTrace tr = sh::trace_move(s.sv, eye, {}, {}, b.at, sh::kTraceNoMonsters, -1);
  return tr.fraction >= 1 || tr.ent == b.ent;
}

// The node a button is pressed (or shot) from: the nearest within reach whose line meets the
// button itself, so a node behind the wall the button is set in is never chosen. A button pressed
// by touch is pressed from a node a standing body reaches it from (its middle within
// kPressRise of the node's): a floor under a raised button sees it but cannot touch it.
constexpr float kPressRise = 72;
int32_t button_node(const sb::NavGraph& g, const ge::shooterbots::BotView& s, const ButtonSpot& b,
                    float reach) {
  // A shot button from a node off any mover first: one on a door's or a stair's travel may stand
  // where nothing is until the button itself is fired (e1m3's shot button that lowers the stairs
  // was given a node on their far side, never reached). A pressed one keeps the nearest (a lift's
  // button is pressed from on the lift).
  for (const bool on_mover : {!b.shoot, true}) {
    int32_t best = -1;
    float best_d = reach;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      if (!on_mover && g.nodes[n].mover >= 0) continue;
      const sh::Vec3f o = g.nodes[n].origin;
      const float d = dist(o, b.at);
      if (d >= best_d || !sees(s, {o.x, o.y, o.z + 22}, b)) continue;
      if (!b.shoot && std::fabs(b.at.z - o.z) > kPressRise) continue;
      best = static_cast<int32_t>(n);
      best_d = d;
    }
    if (best >= 0) return best;
  }
  return sb::nav_nearest(g, b.at);
}

// Within `r` of the box from `lo` to `hi`.
bool near_box(sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi, float r) {
  return p.x >= lo.x - r && p.x <= hi.x + r && p.y >= lo.y - r && p.y <= hi.y + r &&
         p.z >= lo.z - r && p.z <= hi.z + r;
}

// The distance from `p` to the box from `lo` to `hi` (0 inside).
float box_dist(sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi) {
  const float dx = std::fmax(0.f, std::fmax(lo.x - p.x, p.x - hi.x));
  const float dy = std::fmax(0.f, std::fmax(lo.y - p.y, p.y - hi.y));
  const float dz = std::fmax(0.f, std::fmax(lo.z - p.z, p.z - hi.z));
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// (`doorway`) Whether door `d` (its entity text) at rest from `lo` to `hi` comes nearer `p` when
// fired: it moves along its angle (-1 up, -2 down, else a heading) by its size less its lip (8),
// the other way when it starts open (spawnflag 1: it rests at its open place). Such a door shuts
// over the item when fired (a crushing wall), so it keeps nothing at rest.
bool fired_nearer(const sb::EntityBlock& d, sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi) {
  const char* av = sb::block_value(d, "angle");
  const char* lv = sb::block_value(d, "lip");
  const char* sv = sb::block_value(d, "spawnflags");
  const double angle = av ? std::atof(av) : 0;
  sh::Vec3f dir{0, 0, 0};
  if (angle == -1) {
    dir.z = 1;
  } else if (angle == -2) {
    dir.z = -1;
  } else {
    const double a = angle * ge::core::kPi / 180;
    dir.x = static_cast<float>(ge::core::libm::cos(a));
    dir.y = static_cast<float>(ge::core::libm::sin(a));
  }
  if (sv && (std::atoi(sv) & 1) != 0) dir = dir * -1.f;
  const sh::Vec3f size = hi - lo;
  const float travel = std::fabs(dir.x * size.x) + std::fabs(dir.y * size.y) +
                       std::fabs(dir.z * size.z) - (lv ? static_cast<float>(std::atof(lv)) : 8.f);
  const sh::Vec3f move = dir * travel;
  return box_dist(p, lo + move, hi + move) < box_dist(p, lo, hi);
}

// (`doorway`) The nodes the nav's links reach from `from`, leaving out every node and link that
// crosses one of the movers `shut` (a button's doors, which open together).
std::vector<uint8_t> reach_without(const sb::NavGraph& g, const std::vector<int32_t>& from,
                                   const std::vector<int>& shut) {
  const auto is_shut = [&](int32_t m) {
    for (int d : shut)
      if (m == d) return true;
    return false;
  };
  std::vector<uint8_t> seen(g.nodes.size(), 0);
  std::vector<int32_t> open;
  for (int32_t n : from)
    if (n >= 0 && !seen[static_cast<size_t>(n)] &&
        !is_shut(g.nodes[static_cast<size_t>(n)].mover)) {
      seen[static_cast<size_t>(n)] = 1;
      open.push_back(n);
    }
  while (!open.empty()) {
    const sb::NavNode& a = g.nodes[static_cast<size_t>(open.back())];
    open.pop_back();
    for (int32_t q = 0; q < a.links; q++) {
      const sb::NavLink& l = g.links[static_cast<size_t>(a.first_link) + static_cast<size_t>(q)];
      const auto to = static_cast<size_t>(l.to);
      if (seen[to] || is_shut(l.mover) || is_shut(g.nodes[to].mover)) continue;
      seen[to] = 1;
      open.push_back(l.to);
    }
  }
  return seen;
}

}  // namespace

std::vector<std::string> button_fires(const std::vector<sb::EntityBlock>& blocks,
                                      const char* target) {
  std::vector<std::string> names;
  if (!target || !target[0]) return names;
  names.emplace_back(target);
  for (size_t from = 0, depth = 0; from < names.size() && depth < kRelayDepth; depth++) {
    const size_t to = names.size();
    for (; from < to; from++)
      for (const sb::EntityBlock& b : blocks) {
        const char* cn = sb::block_value(b, "classname");
        const char* tn = sb::block_value(b, "targetname");
        const char* next = sb::block_value(b, "target");
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

std::vector<ge::shooterbots::NavErrand> field_errands(const Fighter& f, const sb::NavGraph& g,
                                                      const ge::shooterbots::BotView& s) {
  std::vector<ge::shooterbots::NavErrand> errands;
  for (const ButtonSpot& b : f.button_spots) {
    ge::shooterbots::NavErrand e;
    e.button = b.node;
    // A shot button from every node that sees it within kShotFrom, as a person shoots it from
    // wherever he stands.
    for (size_t n = 0; b.shoot && n < g.nodes.size(); n++) {
      const sh::Vec3f o = g.nodes[n].origin;
      if (static_cast<int32_t>(n) == b.node || dist(o, b.at) >= kShotFrom ||
          !sees(s, {o.x, o.y, o.z + 22}, b))
        continue;
      e.also.push_back(static_cast<int32_t>(n));
    }
    for (size_t k = 0; k < b.way_doors.size() && k < static_cast<size_t>(e.kDoors); k++)
      e.doors[k] = b.way_doors[k];
    e.press_s = b.shoot ? kButtonShot_s : kButtonPress_s;
    e.open_s = b.open_s;
    errands.push_back(e);
  }
  return errands;
}

void buttons_init(Fighter& f, const sb::NavGraph& g, const ge::shooterbots::BotView& s,
                  const char* entities) {
  f.button_spots.clear();
  f.poke = -1;
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  if (!entities || !sb::parse_entity_text(entities, blocks, error)) return;
  const auto entity_on = [&](const char* model) {
    for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
      const ge::shooter::WorldEntity& d = s.world.entities[static_cast<size_t>(e)];
      if (!d.free && d.modelindex > 0 && static_cast<size_t>(d.modelindex) < s.models.size() &&
          s.models[static_cast<size_t>(d.modelindex)] == model)
        return e;
    }
    return -1;
  };
  // (`doorway`) What the spawns reach with every door open: an item reached so and not with a
  // door left shut is behind that door; one reached either way is not (dm2's red armour by the
  // gate a shot button far off opens: the bots ran to the button first on 63 % of their trips).
  const bool doorway = f.features[sb::Feature::doorway];
  std::vector<int32_t> spawns;
  std::vector<uint8_t> open_reach;
  if (doorway) {
    for (const sb::EntityBlock& b : blocks) {
      const char* cn = sb::block_value(b, "classname");
      if (!cn || (std::strcmp(cn, "info_player_deathmatch") != 0 &&
                  std::strcmp(cn, "info_player_start") != 0))
        continue;
      sh::Vec3f o;
      if (parse_vec(sb::block_value(b, "origin"), o)) spawns.push_back(sb::nav_nearest(g, o));
    }
    open_reach = reach_without(g, spawns, {});
  }
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* target = sb::block_value(b, "target");
    const char* model = sb::block_value(b, "model");
    if (!cn || std::strcmp(cn, "func_button") != 0 || !model) continue;
    ButtonSpot bs;
    bs.ent = entity_on(model);
    if (bs.ent < 0) continue;
    bs.at = middle(s.world.entities[static_cast<size_t>(bs.ent)]);
    const char* health = sb::block_value(b, "health");
    bs.shoot = health && std::atof(health) > 0;
    bs.node = button_node(g, s, bs, bs.shoot ? 900.f : 256.f);
    const std::vector<std::string> fired = button_fires(blocks, target);
    const auto fires = [&](const sb::EntityBlock& d) {
      const char* dc = sb::block_value(d, "classname");
      const char* dn = sb::block_value(d, "targetname");
      const char* dm = sb::block_value(d, "model");
      if (!dc || std::strcmp(dc, "func_door") != 0 || !dn || !dm) return -1;
      for (const std::string& n : fired)
        if (n == dn) return entity_on(dm);
      return -1;
    };
    // (`doorway`) What the spawns reach with this button's doors all shut.
    std::vector<uint8_t> shut_reach;
    if (doorway) {
      std::vector<int> mine;
      for (const sb::EntityBlock& d : blocks)
        if (const int door = fires(d); door >= 0) mine.push_back(door);
      shut_reach = reach_without(g, spawns, mine);
    }
    for (const sb::EntityBlock& d : blocks) {
      const int door = fires(d);
      if (door < 0) continue;
      bs.doors.push_back(door);
      // A door's wait (3 s unless it says; -1: it stays open): the button's doors open as long as
      // the shortest.
      // and its travel each way (its speed, 100 unless it says, over its size along the way it
      // moves less its lip, 8 unless it says): passable from soon after it starts to soon after
      // it starts back.
      const char* wait = sb::block_value(d, "wait");
      const float w = wait ? static_cast<float>(std::atof(wait)) : 3.f;
      {
        const ge::shooter::WorldEntity& de = s.world.entities[static_cast<size_t>(door)];
        const char* angle = sb::block_value(d, "angle");
        const char* speed = sb::block_value(d, "speed");
        const char* lip = sb::block_value(d, "lip");
        const float a = angle ? static_cast<float>(std::atof(angle)) : 0.f;
        const sh::Vec3f size = de.absmax - de.absmin;
        float along = 0;
        if (a == -1 || a == -2) {
          along = size.z;
        } else {
          const double r = static_cast<double>(a) * ge::core::kPi / 180.0;
          along = static_cast<float>(std::fabs(ge::core::libm::cos(r) * size.x) +
                                     std::fabs(ge::core::libm::sin(r) * size.y));
        }
        const float travel =
            std::fmax(0.f, along - (lip ? static_cast<float>(std::atof(lip)) : 8.f));
        const float v =
            speed && std::atof(speed) > 0 ? static_cast<float>(std::atof(speed)) : 100.f;
        const float open = w > 0 ? w + travel / v : 0;
        if (open > 0 && (bs.open_s <= 0 || open < bs.open_s)) bs.open_s = open;
      }
      // (`fields`) A WAY when fired: not one that starts open (a bridge's) nor one that hurts more
      // than a door's push (dm2's crusher walls by the pillar armours: a trap, not a way).
      const char* sf = sb::block_value(d, "spawnflags");
      const char* dmg = sb::block_value(d, "dmg");
      if (!(sf && (std::atoi(sf) & 1) != 0) && !(dmg && std::atof(dmg) > kDoorPushDmg))
        bs.way_doors.push_back(door);
      const ge::shooter::WorldEntity& de = s.world.entities[static_cast<size_t>(door)];
      for (size_t i = 0; i < f.items.size(); i++) {
        ItemSpot& it = f.items[i];
        if (!near_box(it.at, de.absmin, de.absmax, kDoorKeeps)) continue;
        if (doorway && fired_nearer(d, it.at, de.absmin, de.absmax)) continue;
        if (doorway && it.node >= 0 && open_reach[static_cast<size_t>(it.node)] &&
            shut_reach[static_cast<size_t>(it.node)])
          continue;  // a way round the button's doors
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

void exits_init(Fighter& f, const sb::NavGraph& g, const ge::shooterbots::BotView& s,
                const char* entities) {
  f.exits.clear();
  f.locked.clear();
  f.locked_rest.clear();
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  if (!entities || !sb::parse_entity_text(entities, blocks, error)) return;
  for (const sb::EntityBlock& b : blocks) {
    const char* cn = sb::block_value(b, "classname");
    const char* model = sb::block_value(b, "model");
    // A door that only something else opens: one with a name (not one that starts open), or a
    // secret door. A touch does not open it, so a route through it waits there for ever.
    if (cn && model && model[0] == '*') {
      const char* name = sb::block_value(b, "targetname");
      const char* sf = sb::block_value(b, "spawnflags");
      const bool start_open = sf && (std::atoi(sf) & 1) != 0;
      if ((std::strcmp(cn, "func_door") == 0 && name && !start_open) ||
          std::strcmp(cn, "func_door_secret") == 0) {
        for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
          const ge::shooter::WorldEntity& d = s.world.entities[static_cast<size_t>(e)];
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

bool button_poke(Fighter& f, const ge::shooterbots::BotView& s, int me, double t) {
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

void button_poke_look(Fighter& f, const ge::shooterbots::BotView& s, int me, double t) {
  f.poke_looked = false;
  if (f.poke < 0 || t >= f.poke_until || f.bot.aiming) return;
  const ButtonSpot& b = f.button_spots[static_cast<size_t>(f.poke)];
  const sh::Vec3f self = s.world.entities[static_cast<size_t>(me)].origin;
  const sh::Vec3f eye{self.x, self.y, self.z + 22};
  if (sees(s, eye, b)) f.poke_seen_t = t;
  if (t - f.poke_seen_t > kPokeLookHold_s) return;
  const sh::Vec3f d = b.at - eye;
  if (std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y) > 900) return;
  f.bot.look = true;
  f.bot.look_at = b.at;
  f.bot.look_by = "poke";
  f.poke_looked = true;
}

namespace {

// Whether a wall stands between `self` and button `b` on the straight line (a button on a ledge
// pressed from its front: walked straight at from the side, bunk1's on the 232 ledge pressed him
// into the wall beside it for good; the route's keys bring him round to its front instead).
bool walled(const ge::shooterbots::BotView& s, int me, sh::Vec3f self, const ButtonSpot& b) {
  const sh::HullTrace tr =
      sh::trace_move(s.sv, self, {0, 0, 0}, {0, 0, 0}, b.at, sh::kTraceNoMonsters, me);
  return tr.fraction < 1 && tr.ent == 0;
}

}  // namespace

void button_poke_cmd(Fighter& f, const ge::shooterbots::BotView& s, int me, double t,
                     sh::MoveCmd& cmd) {
  if (f.poke < 0 || t >= f.poke_until || f.bot.aiming) return;
  const ButtonSpot& b = f.button_spots[static_cast<size_t>(f.poke)];
  const sh::Vec3f self = s.world.entities[static_cast<size_t>(me)].origin;
  const sh::Vec3f eye{self.x, self.y, self.z + 22};
  const bool by_look = f.features[sb::Feature::pokelook];
  if (by_look ? !f.poke_looked : !sees(s, eye, b)) return;
  const sh::Vec3f d = b.at - eye;
  const double flat = std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y);
  if (flat > 900) return;
  const auto on_button = [&]() {
    const float yaw = static_cast<float>(ge::core::libm::atan2(d.y, d.x) * (180.0 / ge::core::kPi));
    const float pitch =
        static_cast<float>(-ge::core::libm::atan2(d.z, flat) * (180.0 / ge::core::kPi));
    float dy = yaw - cmd.angles.y;
    while (dy > 180) dy -= 360;
    while (dy < -180) dy += 360;
    const float dp = pitch - cmd.angles.x;
    return dy * dy + dp * dp <= kPokeOn * kPokeOn;
  };
  if (by_look) {
    // (`pokelook`) The view is the step's, on the look: the trigger only once it is there.
    if (b.shoot && f.features[sb::Feature::pokestand] && flat < 250) {
      cmd.forward = 0;
      cmd.side = 0;
      cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
    } else if (flat < 200 && !walled(s, me, self, b)) {
      cmd.forward = 320;
      cmd.side = 0;
    }
    if (f.stats.commands % 8 == 0 && on_button())
      cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
    return;
  }
  const float was_yaw = cmd.angles.y;
  cmd.angles.y = static_cast<float>(ge::core::libm::atan2(d.y, d.x) * (180.0 / ge::core::kPi));
  cmd.angles.x = static_cast<float>(-ge::core::libm::atan2(d.z, flat) * (180.0 / ge::core::kPi));
  // The route's wish kept on its way under the view turned to the button (the hand reads a game's
  // view's wish as meant under that view: unturned, a hop's side key braked in the air).
  sb::turn_axes(cmd.forward, cmd.side, was_yaw, cmd.angles.y);
  f.bot.set_by = "poke";
  if (b.shoot && f.features[sb::Feature::pokestand]) {
    // (`pokestand`) A button a shot opens is never touched: near it the keys let go, so the view
    // settles on it and the shot goes off (the hand holds the trigger until the view is there; run
    // round under dm2's RA button at 300 units/s it seldom was: 12 s of pokes, no shot).
    if (flat < 250) {
      cmd.forward = 0;
      cmd.side = 0;
      cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonJump);
    }
  } else if (flat < 200 && !walled(s, me, self, b)) {
    cmd.forward = 320;  // into it: a touch presses it
    cmd.side = 0;
  }
  // He fires at any button, whatever opens it (a few shots a second, as a trigger finger does).
  if (f.stats.commands % 8 == 0)
    cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
}

}  // namespace ge::shooterbots
