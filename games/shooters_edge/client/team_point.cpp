// What the player points at for team chat (team_point.hpp).
//
// ge:summary What the player points at for team chat: the pointing table, the choice, the scenes
// ge:keywords point pointing tp_msgpoint point_at point_scene pointing-table line-of-sight
// ge:skill shooter console
// ge:test tests/shooters_edge_team_point_test.cpp
#include "shooters_edge/client/team_point.hpp"

#include <cmath>
#include <cstdlib>
#include <string_view>

#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::client {

namespace con = ge::console;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

namespace {

enum class Group : uint8_t { power, armour, weapon, mega, pack, ammo, rune };

// An item the player can point at, by its model's file (`skin` -1: any).
struct PointRow {
  const char* word;  // its word setting
  std::string_view model;
  int skin;
  Group group;
};

// Both classic games' pointable items; the files never meet, so one table serves either.
constexpr PointRow kRows[] = {
    {"tp_name_quad", "progs/quaddama.mdl", -1, Group::power},
    {"tp_name_pent", "progs/invulner.mdl", -1, Group::power},
    {"tp_name_ring", "progs/invisibl.mdl", -1, Group::power},
    {"tp_name_ga", "progs/armor.mdl", 0, Group::armour},
    {"tp_name_ya", "progs/armor.mdl", 1, Group::armour},
    {"tp_name_ra", "progs/armor.mdl", -1, Group::armour},
    {"tp_name_rl", "progs/g_rock2.mdl", -1, Group::weapon},
    {"tp_name_lg", "progs/g_light.mdl", -1, Group::weapon},
    {"tp_name_gl", "progs/g_rock.mdl", -1, Group::weapon},
    {"tp_name_sng", "progs/g_nail2.mdl", -1, Group::weapon},
    {"tp_name_mh", "maps/b_bh100.bsp", -1, Group::mega},
    {"tp_name_backpack", "progs/backpack.mdl", -1, Group::pack},
    {"tp_name_rockets", "maps/b_rock0.bsp", -1, Group::ammo},
    {"tp_name_rockets", "maps/b_rock1.bsp", -1, Group::ammo},
    {"tp_name_cells", "maps/b_batt0.bsp", -1, Group::ammo},
    {"tp_name_cells", "maps/b_batt1.bsp", -1, Group::ammo},
    {"tp_name_rune1", "progs/end1.mdl", -1, Group::rune},
    {"tp_name_rune2", "progs/end2.mdl", -1, Group::rune},
    {"tp_name_rune3", "progs/end3.mdl", -1, Group::rune},
    {"tp_name_rune4", "progs/end4.mdl", -1, Group::rune},
    {"tp_name_quad", "models/items/quaddama/tris.md2", -1, Group::power},
    {"tp_name_pent", "models/items/invulner/tris.md2", -1, Group::power},
    {"tp_name_ga", "models/items/armor/jacket/tris.md2", -1, Group::armour},
    {"tp_name_ya", "models/items/armor/combat/tris.md2", -1, Group::armour},
    {"tp_name_ra", "models/items/armor/body/tris.md2", -1, Group::armour},
    {"tp_name_rg", "models/weapons/g_rail/tris.md2", -1, Group::weapon},
    {"tp_name_rl", "models/weapons/g_rocket/tris.md2", -1, Group::weapon},
    {"tp_name_hb", "models/weapons/g_hyperb/tris.md2", -1, Group::weapon},
    {"tp_name_bfg", "models/weapons/g_bfg/tris.md2", -1, Group::weapon},
    {"tp_name_cg", "models/weapons/g_chain/tris.md2", -1, Group::weapon},
    {"tp_name_gl", "models/weapons/g_launch/tris.md2", -1, Group::weapon},
    {"tp_name_mh", "models/items/mega_h/tris.md2", -1, Group::mega},
    {"tp_name_backpack", "models/items/pack/tris.md2", -1, Group::pack},
    {"tp_name_rockets", "models/items/ammo/rockets/medium/tris.md2", -1, Group::ammo},
    {"tp_name_cells", "models/items/ammo/cells/medium/tris.md2", -1, Group::ammo},
};

// The classic effect bits a power held lights its holder with (dim light, blue, red).
constexpr int kGlow = 8, kGlowBlue = 64, kGlowRed = 128;
constexpr std::string_view kEyes = "progs/eyes.mdl";
constexpr std::string_view kHead = "progs/h_player.mdl";
constexpr float kNearest = 10;    // ahead of the eye, units
constexpr float kMostMiss = 300;  // from the view's line, units
constexpr float kMissPerAhead = 1.7f;
constexpr float kNear = 375;  // closer: the miss weighted by the distance
constexpr float kPlayerRadius = 27, kPlayerMiddle = 30;
constexpr float kItemRadius = 16;
constexpr float kGlowRadius = 200;  // a glowing or lit thing is seen by its light

const PointRow* row_of(const PointThing& t) {
  for (const PointRow& r : kRows)
    if (r.model == t.model && (r.skin < 0 || r.skin == t.skin)) return &r;
  return nullptr;
}

std::string word(const con::Console& c, std::string_view name) {
  const con::Cvar* v = c.cvars().find(name);
  return v == nullptr ? std::string() : v->value;
}

// Whether `row` is in the pointing set under `deathmatch`.
bool counted(const PointRow& row, int deathmatch) {
  if (row.group == Group::ammo && deathmatch == 4) return false;
  if (row.group == Group::weapon && deathmatch != 1) return false;
  return true;
}

// The middle a thing is seen by: a player's 30 units over his feet; an item box's (a brush
// model's origin is its corner) or a figure's.
sh::Vec3f middle_of(const PointThing& t) {
  if (t.player) return {t.origin.x, t.origin.y, t.origin.z + kPlayerMiddle};
  const std::string_view m = t.model;
  if (m.size() > 4 && m.substr(m.size() - 4) == ".bsp")
    return {t.origin.x + 16, t.origin.y + 16, t.origin.z + 12};
  if (m.size() > 4 && m.substr(m.size() - 4) == ".md2") return t.origin;  // its box round it
  return {t.origin.x, t.origin.y, t.origin.z + 16};
}

// Whether the eye sees the middle or a point `radius` out from it on any axis.
bool in_sight(sh::Vec3f eye, sh::Vec3f mid, float radius, const ClearLine& clear) {
  if (!clear || clear(eye, mid)) return true;
  const sh::Vec3f around[] = {{radius, 0, 0},  {-radius, 0, 0}, {0, radius, 0},
                              {0, -radius, 0}, {0, 0, radius},  {0, 0, -radius}};
  for (const sh::Vec3f& d : around)
    if (clear(eye, {mid.x + d.x, mid.y + d.y, mid.z + d.z})) return true;
  return false;
}

}  // namespace

Pointed point_at(const con::Console& c, const PointScene& scene, const ClearLine& clear,
                 ItemSet allowed) {
  Pointed out;
  if (!scene.on) return out;
  const sh::Vec3f f = sh::angle_vectors(scene.angles).forward;
  float best = 0;
  const PointThing* chosen = nullptr;
  const PointRow* chosen_row = nullptr;
  for (const PointThing& t : scene.things) {
    const PointRow* row = nullptr;
    if (t.player) {
      if (t.model == kHead) continue;
      const char* kind = scene.spectator    ? "enemy"
                         : t.model == kEyes ? "eyes"
                         : t.teammate       ? "teammate"
                                            : "enemy";
      if ((allowed & item_flag(kind)) == 0) continue;
    } else {
      row = row_of(t);
      if (row == nullptr || !counted(*row, scene.deathmatch)) continue;
      if (const ItemSet b = item_of_word(row->word); b != 0 && (allowed & b) == 0) continue;
    }
    const sh::Vec3f mid = middle_of(t);
    const sh::Vec3f d = {mid.x - scene.eye.x, mid.y - scene.eye.y, mid.z - scene.eye.z};
    const float ahead = d.x * f.x + d.y * f.y + d.z * f.z;
    if (ahead < kNearest) continue;
    const float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
    const float miss = std::sqrt(std::fmax(0.0f, dist2 - ahead * ahead));
    if (miss > kMostMiss || miss > ahead * kMissPerAhead) continue;
    const float dist = std::sqrt(dist2);
    const float rank = dist < kNear ? miss * (dist * 0.0016f + 0.3f) : miss;
    if (chosen != nullptr && rank >= best) continue;
    const float radius = t.quaded || t.pented || t.lit ? kGlowRadius
                         : t.player                    ? kPlayerRadius
                                                       : kItemRadius;
    if (!in_sight(scene.eye, mid, radius, clear)) continue;
    best = rank;
    chosen = &t;
    chosen_row = row;
  }
  if (chosen == nullptr) return out;
  out.any = true;
  out.at = chosen->origin;
  out.quaded = chosen->quaded;
  out.pented = chosen->pented;
  if (chosen_row != nullptr) {
    out.item = chosen_row->word;
    out.thing = word(c, chosen_row->word);
    return out;
  }
  std::string who;
  if (scene.spectator) {
    who = chosen->name;
  } else if (chosen->model == kEyes) {
    out.eyes = true;
    who = word(c, "tp_name_eyes");
  } else if (chosen->teammate) {
    out.teammate = true;
    const std::string w = word(c, "tp_name_teammate");
    who = w.empty() ? chosen->name : w;
  } else {
    out.enemy = true;
    who = word(c, "tp_name_enemy");
  }
  std::string lead;
  if (chosen->quaded) lead += word(c, "tp_name_quaded") + " ";
  if (chosen->pented) lead += word(c, "tp_name_pented") + " ";
  out.thing = lead + who;
  return out;
}

PointScene point_scene(const DemoState& ds) {
  PointScene s;
  if (ds.map.empty() || ds.intermission) return s;
  s.on = true;
  s.eye = ds.eye;
  s.angles = ds.angles;
  bool teamplay = false;
  for (const auto& [key, value] : ds.serverinfo) {
    if (key == "deathmatch") s.deathmatch = std::atoi(value.c_str());
    if (key == "teamplay") teamplay = std::atoi(value.c_str()) != 0;
  }
  const bool q2 = ds.protocol == 34;  // its effect bits are not the glows
  const auto model_of = [&ds](int m) {
    return m > 0 && size_t(m) < ds.models.size() ? ds.models[size_t(m)] : std::string();
  };
  const auto thing_of = [&](const DemoEntity& e) {
    PointThing t;
    t.model = model_of(e.model);
    t.skin = e.skin;
    t.origin = e.origin;
    if (!q2) {
      t.quaded = (e.effects & kGlowBlue) != 0;
      t.pented = (e.effects & kGlowRed) != 0;
      t.lit = (e.effects & kGlow) != 0;
    }
    return t;
  };
  for (const DemoEntity& e : ds.entities)
    if (e.present && e.model > 0) s.things.push_back(thing_of(e));
  const size_t me = size_t(ds.player);
  s.spectator = me < ds.spectator.size() && ds.spectator[me];
  for (size_t slot = 0; slot < ds.players.size() && slot < ds.names.size(); slot++) {
    const DemoEntity& p = ds.players[slot];
    if (slot == me || !p.present || p.dead || p.gib || p.model <= 0 || ds.spectator[slot]) continue;
    PointThing t = thing_of(p);
    t.player = true;
    t.name = ds.names[slot];
    t.teammate = teamplay && me < ds.teams.size() && ds.teams[slot] == ds.teams[me];
    s.things.push_back(std::move(t));
  }
  return s;
}

PointScene point_scene(const Listen& l) {
  PointScene s;
  if (!l.server || !l.map) return s;
  const sv::RulesCommon& rc = *l.server;
  const PlayerState me = player_state(l);
  s.on = true;
  s.eye = {me.origin.x + me.view_ofs.x, me.origin.y + me.view_ofs.y, me.origin.z + me.view_ofs.z};
  s.angles = l.view;  // the window's own view, ahead of the server's last command
  if (const con::Cvar* dm = rc.cvars.find("deathmatch")) s.deathmatch = int(dm->number);
  const con::Cvar* tp = rc.cvars.find("teamplay");
  const bool teamplay = tp != nullptr && tp->on();
  const std::string mine = me.entity > 0 && me.entity <= sh::kMaxClients
                               ? sv::info_value(rc.clients[size_t(me.entity - 1)].userinfo, "team")
                               : std::string();
  // A program's entities carry the classic effect bits; rules of our own carry their own, so
  // their glows are not read.
  const bool qc = l.rules != nullptr && sv::qc_of(*l.rules) != nullptr;
  const int models = int(rc.models.size());
  for (int e = 1; e < rc.sv.num_edicts; e++) {
    const sh::Edict& ed = rc.sv.edicts[size_t(e)];
    if (ed.free || ed.modelindex <= 0 || ed.modelindex >= models || e == me.entity) continue;
    if (!model_shown(l, e)) continue;  // a taken item
    PointThing t;
    t.model = rc.models[size_t(ed.modelindex)];
    t.origin = ed.origin;
    if (qc) {
      t.skin = int(field_named(l, e, "skin"));
      const int fx = int(field_named(l, e, "effects"));
      t.quaded = (fx & kGlowBlue) != 0;
      t.pented = (fx & kGlowRed) != 0;
      t.lit = (fx & kGlow) != 0;
    } else if (size_t(e) < rc.world.entities.size()) {
      t.skin = rc.world.entities[size_t(e)].skin;
    }
    if (e <= sh::kMaxClients) {
      const auto& info = rc.clients[size_t(e - 1)].userinfo;
      t.player = true;
      t.name = sv::info_value(info, "name");
      t.teammate = teamplay && !mine.empty() && sv::info_value(info, "team") == mine;
    }
    s.things.push_back(std::move(t));
  }
  return s;
}

}  // namespace shooters_edge::client
