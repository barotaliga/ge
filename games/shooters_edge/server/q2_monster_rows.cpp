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
#include <vector>

#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_monster_pain.hpp"
#include "shooters_edge/server/q2_monster_steps.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
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
    for (const char* name : kKinds[k].sounds) add_sound(name);
    for (const Attack& a : kKinds[k].missile) add_sound(a.cry);
    for (const char* name : kKinds[k].chant) add_sound(name);
    for (const char* name : monster_step_sounds(kKinds[k].classname, steps_on)) add_sound(name);
    const auto add_to = [](void* to, const char* name) {
      std::vector<std::string>& list = *static_cast<std::vector<std::string>*>(to);
      if (std::find(list.begin(), list.end(), name) == list.end()) list.emplace_back(name);
    };
    monster_cue_sounds(looks_like(kKinds[k]), add_to, &s.sounds);
    monster_pain_sounds(looks_like(kKinds[k]), add_to, &s.sounds);
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
    if (kKinds[k].medic && !kKinds[k].repairs)
      for (const char* name : {"medic/medatck2.wav", "medic/medatck4.wav"}) add_sound(name);
    if (kKinds[k].end == End::launch) add_kind(kind_of(s, "monster_makron"));
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

namespace monster_ai {

constexpr const char* kBeam = "monster_beam";

bool beams_of(const Kind& k) {
  if (k.repairs) return true;
  for (const Attack& a : k.missile)
    if (a.fire == Fire::laser) return true;
  return false;
}

void beams_spawn(Q2Server& s) {
  for (Q2Monster& m : s.monsters) {
    if (!beams_of(kKinds[m.kind])) continue;
    m.beam = static_cast<int>(s.entities.size());
    for (int i = 0; i < 2; i++) {
      Q2Entity b;
      b.classname = kBeam;
      b.hidden = true;
      b.owner = m.entity;
      s.entities.push_back(b);
    }
  }
}

void beam_show(Q2Server& s, Q2Monster& m, int i, sh::Vec3f from, sh::Vec3f to, bool heal) {
  if (m.beam < 0 || i < 0 || i > 1) return;
  m.beam_from[i] = from;
  m.beam_to[i] = to;
  m.beam_heal = heal;
  m.beam_until = s.level_time + 0.15;
  if (i == 0) m.beam_from[1] = m.beam_to[1] = from;  // the second eye's, if it fires, after
}

}  // namespace monster_ai

namespace {

// Each monster's beams: the red laser (palette 242, 242, 240, 240) or the blue healing beam (243,
// 243, 241, 241), 2 wide, seen through, from its eye to where it stopped, while one is shown.
void beam_rows(Q2Server& s) {
  RulesWorld& w = s.world;
  for (const Q2Monster& m : s.monsters) {
    if (m.beam < 0) continue;
    for (int i = 0; i < 2; i++) {
      const size_t row = size_t(map_row(m.beam + i));
      if (row >= w.entities.size()) continue;
      WorldEntity& r = w.entities[row];
      const bool shown = !m.dead && s.level_time < m.beam_until &&
                         (m.beam_to[i].x != m.beam_from[i].x ||
                          m.beam_to[i].y != m.beam_from[i].y || m.beam_to[i].z != m.beam_from[i].z);
      r.free = !shown;
      if (!shown) continue;
      r.origin = m.beam_from[i];
      r.beam_end = m.beam_to[i];
      r.modelindex = 0;
      r.render_fx = kSqBeamRender | kSqTranslucentRender;
      r.frame = 2;
      r.skin = static_cast<int>(m.beam_heal ? 0xF3F3F1F1u : 0xF2F2F0F0u);
    }
  }
}

}  // namespace

void monster_rows(Q2Server& s) {
  torso_rows(s);
  beam_rows(s);
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
    if (m.flies_at > 0 && s.level_time >= m.flies_at && s.level_time < m.flies_at + 60) {
      loop = "infantry/inflies1.wav";
      r.effects = kSqFliesEffect;
    }
    if (!m.dead && s.level_time < m.screen_flash)
      r.effects |= kSqPowerScreenEffect;  // a blow taken
    if (loop != nullptr && !e.gone) {
      const auto at = std::find(s.sounds.begin(), s.sounds.end(), loop);
      if (at != s.sounds.end()) r.loop = static_cast<int>(at - s.sounds.begin());
    }
    // The damaged skin below half health; a prisoner one of three by his body (none on a cross).
    const Kind& k = kKinds[m.kind];
    const bool hurt = e.health < e.max_health / 2;
    r.skin = hurt && k.hurt_skin >= 0 ? k.hurt_skin : k.skin + (hurt ? 1 : 0);
    if (k.heals)  // the gekk: its second skin below half, its third below a quarter and dead
      r.skin = m.dead || e.health < e.max_health / 4 ? 2 : hurt ? 1 : 0;
    r.render_fx = 0;
    if (!m.dead && s.level_time < m.shield_flash) {  // a power shield's blow: the green shell
      r.effects |= kSqColourShellEffect;
      r.render_fx = kSqShieldShellRender;
    }
    if (kKinds[m.kind].classname == std::string_view("misc_insane"))
      r.skin = (e.spawnflags & 8) != 0 ? 0 : int((uint32_t(m.entity) * 2654435761u >> 16) % 3u);
    // The commander's body slumps from frame 1 to 24, a frame every 0.1 s from 0.1 s after a use.
    if (kKinds[m.kind].prop && m.used_at >= 0)
      r.frame = std::clamp(static_cast<int>((s.level_time - m.used_at) / 0.1), 0, 24);
    else if (sq_monster_poses(looks_like(kKinds[m.kind])) != nullptr)
      r.frame = m.pose.frame;
  }
}

}  // namespace shooters_edge::server
