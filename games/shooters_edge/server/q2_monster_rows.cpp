// Quake II's monsters as the world's rows and their content (q2_monsters.hpp): the models and
// sounds each kind on the map needs, listed at load, and each monster's row a frame (its place,
// box, frame, skin, the loop it carries and its effects).
//
// ge:summary Quake II's monsters' precache and world rows: models, sounds, skin, loops, flies
// ge:keywords q2 monsters precache world-rows monster-rows skin loop-sound flies
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_monster_pain.hpp"
#include "shooters_edge/server/q2_monster_steps.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_powers.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

using namespace roster;

namespace {

int kind_of(const Q2Server& s, std::string_view classname) {
  return kind_named(classname, s.opt.pack);
}

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

}  // namespace

void monster_precache(Q2Server& s) {
  const auto model = [&s](const char* name) {
    const auto found = std::find(s.models.begin(), s.models.end(), name);
    if (found != s.models.end()) return static_cast<int>(found - s.models.begin());
    s.models.emplace_back(name);
    return static_cast<int>(s.models.size()) - 1;
  };
  const auto add_sound = [&s](const char* name) {
    if (name != nullptr && std::find(s.sounds.begin(), s.sounds.end(), name) == s.sounds.end())
      s.sounds.emplace_back(name);
  };
  const bool steps_on = s.cvars.find("g_monsterfootsteps")->integer() != 0;
  const auto add_kind = [&](int k) {
    for (const char* name : {kKinds[k].sight, kKinds[k].pain, kKinds[k].death}) add_sound(name);
    if (kKinds[k].sight != nullptr && std::string_view(kKinds[k].sight) == "soldier/solsght1.wav")
      add_sound("soldier/solsrch1.wav");  // the soldiers' other sight call
    if (std::string_view(kKinds[k].classname) == "misc_insane") {
      for (const char* name : kInsaneDeaths) add_sound(name);
      for (const char* name : kInsanePains) add_sound(name);
    }
    for (const char* name : {kKinds[k].idle, kKinds[k].loop, kKinds[k].search[0],
                             kKinds[k].search[1], kKinds[k].search[2]})
      add_sound(name);
    for (const char* name : monster_step_sounds(kKinds[k].classname, steps_on)) add_sound(name);
    const auto add_to = [](void* to, const char* name) {
      std::vector<std::string>& list = *static_cast<std::vector<std::string>*>(to);
      if (std::find(list.begin(), list.end(), name) == list.end()) list.emplace_back(name);
    };
    monster_cue_sounds(looks_like(kKinds[k]), add_to, &s.sounds);
    monster_pain_sounds(pain_class(kKinds[k]), add_to, &s.sounds);
    const std::string_view name = kKinds[k].classname;
    if (name == "monster_infantry" || name == "monster_mutant") add_sound("infantry/inflies1.wav");
    if (name == "monster_jorg") add_sound("boss3/w_loop.wav");
    if (name == "monster_makron") add_sound("makron/spine.wav");
    return model(kKinds[k].model);
  };
  for (Q2Entity& e : s.entities) {
    const int k = kind_of(s, e.classname);
    if (k < 0 || e.gone || (deathmatch(s) && !kKinds[k].keep)) continue;
    e.modelindex = add_kind(k);
    if (kKinds[k].medic)
      for (const char* name : {"medic/medatck2.wav", "medic/medatck4.wav",
                               "medic_commander/medatck2c.wav", "medic_commander/medatck4a.wav"})
        if (kKinds[k].pack == Q2Pack::ground_zero || std::string_view(name).starts_with("medic/"))
          add_sound(name);
    if (kKinds[k].end == End::launch) add_kind(kind_of(s, "monster_makron"));
    if (kKinds[k].classname == std::string_view("monster_medic_commander")) {  // its guards
      for (const char* name : monster_ai::kCommanderGroup)
        if (kind_of(s, name) >= 0) add_kind(kind_of(s, name));
      model(monster_ai::kSpawnGrow);
      model("models/items/spawngro/tris.md2");
      add_sound("medic_commander/monsterspawn1.wav");
    }
    if (kKinds[k].classname == std::string_view("monster_turret")) {  // its guns' and its debris
      for (const char* name : {"soldier/solatck2.wav", "misc/lasfly.wav", "infantry/infatck1.wav",
                               "chick/chkatck2.wav", "weapons/rockfly.wav", "world/dr_short.wav"})
        add_sound(name);
      model("models/objects/debris1/tris.md2");
      model("models/monsters/turretbase/tris.md2");
    }
    if (monster_ai::gz_boss(kKinds[k].classname)) {  // its brood, its grows, gibs and sounds
      const monster_ai::BossContent c = monster_ai::gz_boss_content(kKinds[k].classname);
      for (const char* name : c.kinds)
        if (name != nullptr && kind_of(s, name) >= 0) add_kind(kind_of(s, name));
      for (const char* name : c.models)
        if (name != nullptr) model(name);
      for (const char* name : c.sounds) add_sound(name);
    }
    if (kKinds[k].prop && kKinds[k].keep)
      for (const char* name : {"tank/pain.wav", "tank/thud.wav"}) add_sound(name);
  }
  for (const Q2Entity& e : s.entities) {  // what the spawners make
    if (e.classname != "target_spawner" || e.gone) continue;
    const int k = kind_of(s, e.target);
    if (k >= 0 && !(deathmatch(s) && !kKinds[k].keep)) add_kind(k);
  }
}

namespace {

constexpr const char* kTorso = "makron_torso";

// The Makron's torso, once he has fallen whole: it lies 84 units along the world's -y from him,
// facing his way, and writhes through frames 346 to 364 a frame each 0.1 s from 0.2 s on, for the
// rest of the level, with makron/spine.wav looping. It is never solid and never hurt.
void torso_rows(Q2Server& s) {
  RulesWorld& w = s.world;
  const auto spine = std::find(s.sounds.begin(), s.sounds.end(), "makron/spine.wav");
  for (size_t i = 0; i < s.entities.size(); i++) {
    Q2Entity& t = s.entities[i];
    if (t.classname != kTorso || t.gone) continue;
    const int m = monster_index(s, t.owner);
    if (t.hidden && m >= 0 && s.monsters[size_t(m)].dead && !s.entities[size_t(t.owner)].gone) {
      const Q2Entity& b = s.entities[size_t(t.owner)];
      t.hidden = false;
      t.origin = b.origin - sh::Vec3f{0, 84, 0};
      t.angles = b.angles;
      t.modelindex = b.modelindex;
      t.until = s.level_time;
    }
    const size_t row = size_t(map_row(static_cast<int>(i)));
    if (row >= w.entities.size()) continue;
    WorldEntity& r = w.entities[row];
    r.free = t.hidden;
    if (t.hidden) continue;
    r.origin = t.origin;
    r.angles = t.angles;
    r.mins = t.mins;
    r.maxs = t.maxs;
    r.modelindex = t.modelindex;
    const int k = static_cast<int>((s.level_time - t.until) * 10 + 0.001);
    r.frame = 346 + (k < 2 ? 0 : (k - 1) % 19);
    r.loop = spine != s.sounds.end() ? static_cast<int>(spine - s.sounds.begin()) : 0;
    // Ground Zero: a red shell while a medic raises him.
    const bool raised =
        s.opt.pack == Q2Pack::ground_zero && m >= 0 && s.monsters[size_t(m)].claimed;
    r.effects = raised ? kSqColourShellEffect : 0;
    r.render_fx = raised ? kSqRedShellRender : 0;
  }
}

}  // namespace

void monster_torsos(Q2Server& s) {
  const size_t n = s.entities.size();
  for (size_t i = 0; i < n; i++) {
    if (s.entities[i].classname != "monster_makron" || s.entities[i].gone) continue;
    Q2Entity t;
    t.classname = kTorso;
    t.hidden = true;
    t.owner = static_cast<int>(i);
    t.mins = {-8, -8, 0};
    t.maxs = {8, 8, 8};
    s.entities.push_back(t);
  }
}

void monster_rows(Q2Server& s) {
  torso_rows(s);
  RulesWorld& w = s.world;
  for (const Q2Monster& m : s.monsters) {
    const Q2Entity& e = s.entities[size_t(m.entity)];
    const size_t row = size_t(map_row(m.entity));
    if (row >= w.entities.size()) continue;
    WorldEntity& r = w.entities[row];
    r.free = e.gone || e.hidden;
    r.origin = e.origin;
    r.angles = {0, e.angles.y, 0};
    r.velocity = m.velocity;
    r.mins = e.mins;
    r.maxs = e.maxs;
    r.absmin = e.origin + e.mins;
    r.absmax = e.origin + e.maxs;
    r.health = float(e.health);
    r.modelindex = e.modelindex;
    r.loop = 0;  // the loop it carries (a flier's engine), till it is thrown apart
    // Jorg's guns whirr while they fire; flies buzz round a corpse.
    const char* loop = kKinds[m.kind].loop;
    if (kKinds[m.kind].classname == std::string_view("monster_jorg") &&
        m.pose.act == SqMonsterPose::Act::attack && m.pose.span.first == 0 && m.pose.frame <= 13 &&
        !m.dead)
      loop = "boss3/w_loop.wav";
    r.effects = 0;
    r.render_fx = 0;
    if (m.flies_at > 0 && s.level_time >= m.flies_at && s.level_time < m.flies_at + 60) {
      loop = "infantry/inflies1.wav";
      r.effects = kSqFliesEffect;
    }
    if (!m.dead && s.level_time < m.screen_flash)
      r.effects |= kSqPowerScreenEffect;  // a blow taken
    if (!m.dead &&
        s.opt.pack == Q2Pack::ground_zero) {  // its power-ups' shells, blinking at the end
      const std::pair<const char*, double> held[] = {
          {"item_quad", m.boss.quad_until},
          {"item_double", m.boss.double_until},
          {"item_invulnerability", m.boss.invulnerable_until}};
      for (const SqPower& pw : sq_powers())
        for (const auto& [item, until] : held)
          if (std::string_view(pw.item) == item && sq_power_shown(until - s.level_time)) {
            r.effects |= pw.effects;
            r.render_fx |= pw.render;
          }
    }
    if (loop != nullptr && !e.gone) {
      const auto at = std::find(s.sounds.begin(), s.sounds.end(), loop);
      if (at != s.sounds.end()) r.loop = static_cast<int>(at - s.sounds.begin());
    }
    // The damaged skin below half health; a prisoner one of three by his body (none on a cross).
    r.skin = kKinds[m.kind].skin + (e.health < e.max_health / 2 ? 1 : 0);
    if (kKinds[m.kind].classname == std::string_view("misc_insane"))
      r.skin = (e.spawnflags & 8) != 0 ? 0 : int((uint32_t(m.entity) * 2654435761u >> 16) % 3u);
    // The commander's body slumps from frame 1 to 24, a frame every 0.1 s from 0.1 s after a use.
    if (kKinds[m.kind].prop && m.used_at >= 0)
      r.frame = std::clamp(static_cast<int>((s.level_time - m.used_at) / 0.1), 0, 24);
    else if (sq_monster_poses(looks_like(kKinds[m.kind])) != nullptr)
      r.frame = m.pose.frame;
    monster_ai::gz_row(s, m, r);  // Ground Zero's grow before a guard comes
  }
}

}  // namespace shooters_edge::server
