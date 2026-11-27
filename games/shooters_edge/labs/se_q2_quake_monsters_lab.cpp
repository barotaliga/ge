// LAB `se-q2-quake-monsters`: do Quake's monsters under Quake II's rules (classic remix's
// `cr_monsters q1`) decide by Quake's mind (server/q1_monster_mind.hpp)? Each of Quake's kinds
// stands alone in a hall built here (no game files) with the player 400 units off. Facing him: when
// it notices him, when its first attack lands, how often it hits him in 20 s, and his health at its
// lowest. Facing away 600 off while he fires the other way for a second: whether it heard him (one
// of Quake II's walks to the noise; one of Quake's hears nothing). Facing him while he fires the
// blaster at it: how often it flinches. Every number is deterministic.
//
// ge:summary Lab: Quake's monsters under Quake II's rules, each kind's notice, hits and hearing
// ge:keywords lab q2 cr_monsters remix quake-monsters q1-mind se-q2-quake-monsters hearing pain
// ge:keywords flinch
// ge:skill shooter labs
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

using Pairs = std::vector<std::pair<std::string, std::string>>;
constexpr int kSecond = 77;  // host ticks of 13 ms

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A long floored hall, the player at its west end facing east, and one monster.
std::shared_ptr<const br::Map> hall(const Pairs& monster, std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity(monster));
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

struct Hall {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
};

std::unique_ptr<Hall> open(const Pairs& monster, const std::string& skill, std::string& error) {
  auto h = std::make_unique<Hall>();
  h->r = sv::q2_runner(sv::make_q2_server({}));
  h->r.console("deathmatch 0");
  h->r.console("skill " + skill);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(monster, error);
  if (map.brushes == nullptr || !h->r.start(map, error)) return nullptr;
  h->r.connect(0, "Ana", false, false);
  return h;
}

void tick(Hall& h, uint8_t buttons, float yaw) {
  sh::Packet pk;
  pk.client = 0;
  pk.sequence = ++h.sequence;
  sh::MoveCmd c;
  c.msec = 13;
  c.buttons = buttons;
  c.angles = {0, yaw, 0};
  pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
  h.r.frame(&pk, 1);
}

constexpr const char* kKinds[] = {"monster_army",   "monster_dog",         "monster_enforcer",
                                  "monster_knight", "monster_hell_knight", "monster_ogre",
                                  "monster_wizard", "monster_demon1",      "monster_shambler"};

void run(gl::Lab& lab) {
  const std::string skill = lab.arg("skill");
  const int seconds = static_cast<int>(lab.arg_num("seconds"));
  gl::LabTable t{
      "each of Quake's kinds alone with the player under Quake II's rules",
      {"kind", "noticed (s)", "first hit (s)", "hits", "lowest health", "heard", "flinches"},
      {}};
  int total_hits = 0, heard_any = 0, total_flinches = 0;
  for (const char* kind : kKinds) {
    std::string error;
    auto h = open({{"classname", kind}, {"origin", "-200 0 24"}, {"angle", "180"}}, skill, error);
    if (!h) {
      lab.fail("se-q2-quake-monsters: " + error, "next: read the reason above");
      return;
    }
    for (int i = 0; i < 10; i++) tick(*h, 0, 0);
    float noticed = -1, first = -1, lowest = 100;
    int hits = 0;
    float last = h->s().players[0].health;
    for (int i = 0; i < seconds * kSecond && h->s().players[0].alive; i++) {
      tick(*h, 0, 0);
      const float now = float(i + 1) / float(kSecond);
      if (noticed < 0 && !h->s().monsters.empty() && h->s().monsters[0].enemy == 0) noticed = now;
      const float hp = h->s().players[0].health;
      if (hp < last) {
        hits++;
        if (first < 0) first = now;
      }
      last = hp;
      lowest = std::min(lowest, hp);
    }
    // Facing away 600 off: he fires the other way for a second.
    auto away = open({{"classname", kind}, {"origin", "0 0 24"}, {"angle", "0"}}, skill, error);
    if (!away) {
      lab.fail("se-q2-quake-monsters: " + error, "next: read the reason above");
      return;
    }
    for (int i = 0; i < 10; i++) tick(*away, 0, 0);
    for (int i = 0; i < kSecond; i++) tick(*away, sh::kButtonAttack, 180);
    const sv::Q2Monster& m = away->s().monsters[0];
    const bool heard = m.enemy != -1 || m.sound_target;
    // Facing him 400 off while he fires the blaster at it: how often it flinches.
    auto shot =
        open({{"classname", kind}, {"origin", "-200 0 24"}, {"angle", "180"}}, skill, error);
    if (!shot) {
      lab.fail("se-q2-quake-monsters: " + error, "next: read the reason above");
      return;
    }
    int flinches = 0;
    bool was = false;
    for (int i = 0; i < 10; i++) tick(*shot, 0, 0);
    for (int i = 0; i < seconds * kSecond && shot->s().players[0].alive; i++) {
      tick(*shot, sh::kButtonAttack, 0);
      if (shot->s().monsters.empty() || shot->s().monsters[0].dead) break;
      const bool now = shot->s().monsters[0].pain_hold > shot->s().level_time;
      flinches += (now && !was) ? 1 : 0;
      was = now;
    }
    total_flinches += flinches;
    total_hits += hits;
    heard_any += heard ? 1 : 0;
    const auto secs = [](float v) {
      return v < 0 ? std::string("never") : ge::core::num::to_fixed(v, 2);
    };
    t.rows.push_back({kind, secs(noticed), secs(first), std::to_string(hits),
                      std::to_string(static_cast<int>(lowest)), heard ? "yes" : "no",
                      std::to_string(flinches)});
    lab.metric(std::string("quake_monsters.") + kind + ".flinches", flinches, "count");
    lab.metric(std::string("quake_monsters.") + kind + ".hits", hits, "count");
    lab.metric(std::string("quake_monsters.") + kind + ".first_hit", first, "s");
    lab.metric(std::string("quake_monsters.") + kind + ".noticed", noticed, "s");
  }
  lab.table(t);
  lab.metric("quake_monsters.hits", total_hits, "count");
  lab.metric("quake_monsters.heard", heard_any, "count");
  lab.metric("quake_monsters.flinches", total_flinches, "count");
}

const gl::LabSpec kSpec{
    "se-q2-quake-monsters",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/server (q2_monsters.cpp over q1_monster_mind.hpp)",
    "shooter",
    "Do Quake's monsters under Quake II's rules notice, attack, hear and flinch as Quake's do?",
    {
        {"skill", "1", "the skill (1: medium)"},
        {"seconds", "20", "how long each kind faces the player, s"},
    },
    "--seconds=6",
    run,
};
GE_LAB(kSpec);

}  // namespace
