// Tests: Quake's own deathmatch rules on the arena (`localinfo rules q1`), one by one, as a player
// reads them: a door's crush credited to whoever opened it, the pentagram against a telefrag, the
// quad and ring dropped by the server info keys `dq` and `dr` and taken up again, the switch to the
// best weapon after ammunition, the OctaPower's warning, a spectator's impulse 1, a switching light
// and the fluorescent lights' sounds, a body that slows on the ground; and the server's own lines:
// the operator's chat, the pause words, "Server is paused." to one who joins, the release when the
// last player leaves and the refused suicide. Each rule off leaves the arena's own as it was.
//
// ge:summary Tests: Quake's deathmatch rules on the arena, rule by rule, and the server's lines
// ge:keywords test arena quake-rules rules-q1 squish door-crush pentagram telefrag satans-power
// ge:keywords dq dr dropped-power octapower spectator-impulse light-switch fluoro operator-say
// ge:keywords pause server-is-paused pause-released suicide-refused item_sigil rune serverflags
// ge:keywords func_episodegate func_bossgate trigger_onlyregistered misc_teleporttrain
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/file.hpp"
#include "progs/native.hpp"
#include "shooters_edge/profiles/words.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge_qc_fixture.hpp"

using qcfix::ent;
using qcfix::Fixture;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;
namespace pf = shooters_edge::profiles;

namespace {

constexpr float kItShotgun = 1;
constexpr float kItNailgun = 4;
constexpr float kItRing = 524288;
constexpr float kItPent = 1048576;
constexpr float kItQuad = 4194304;
constexpr float kFlOnground = 512;

const pg::Progs& arena() {
  static const pg::Progs p = [] {
    pg::Progs out;
    std::string error;
    CHECK_MSG(sv::baseline_mod(out, error), error);
    return out;
  }();
  return p;
}

std::string entities() {
  return std::string(sv::kArenaEntities) +
         "{\n\"classname\" \"info_teleport_destination\"\n\"targetname\" \"d\"\n"
         "\"origin\" \"0 -256 24\"\n\"angle\" \"90\"\n}\n"
         "{\n\"classname\" \"item_spikes\"\n\"origin\" \"160 96 24\"\n}\n"
         "{\n\"classname\" \"light\"\n\"targetname\" \"sw\"\n\"style\" \"33\"\n"
         "\"spawnflags\" \"1\"\n\"origin\" \"0 64 200\"\n}\n"
         "{\n\"classname\" \"light_fluoro\"\n\"origin\" \"0 128 200\"\n}\n"
         "{\n\"classname\" \"light_fluorospark\"\n\"origin\" \"0 192 200\"\n}\n"
         "{\n\"classname\" \"item_artifact_super_damage\"\n\"origin\" \"-160 300 24\"\n}\n"
         "{\n\"classname\" \"item_artifact_invisibility\"\n\"origin\" \"-160 -300 24\"\n}\n"
         "{\n\"classname\" \"trigger_multiple\"\n\"model\" \"*2\"\n\"health\" \"10\"\n"
         "\"target\" \"sw\"\n\"wait\" \"1\"\n}\n"
         "{\n\"classname\" \"func_door\"\n\"model\" \"*1\"\n\"angle\" \"-1\"\n}\n"
         "{\n\"classname\" \"item_sigil\"\n\"spawnflags\" \"2\"\n\"target\" \"sw\"\n"
         "\"origin\" \"200 -200 24\"\n}\n"
         "{\n\"classname\" \"func_episodegate\"\n\"model\" \"*1\"\n\"spawnflags\" \"1\"\n}\n"
         "{\n\"classname\" \"func_bossgate\"\n\"model\" \"*1\"\n}\n"
         "{\n\"classname\" \"trigger_onlyregistered\"\n\"model\" \"*2\"\n"
         "\"message\" \"Not in this version\"\n\"target\" \"sw\"\n}\n"
         "{\n\"classname\" \"misc_teleporttrain\"\n\"target\" \"tc\"\n\"origin\" \"0 0 300\"\n}\n"
         "{\n\"classname\" \"path_corner\"\n\"targetname\" \"tc\"\n\"target\" \"tc\"\n"
         "\"origin\" \"256 0 300\"\n}\n";
}

// The console a test starts with: Quake's words, then its own lines (Quake's rules unless `own`).
std::vector<std::string> quake(std::vector<std::string> more, bool rules = true) {
  std::vector<std::string> c = pf::word_lines(pf::kQ1ServerWords);
  if (rules) c.emplace_back("localinfo rules q1");
  for (std::string& l : more) c.push_back(std::move(l));
  return c;
}

struct Rig {
  Fixture fx;
  explicit Rig(const std::vector<std::string>& console, const std::string& more = "")
      : fx((entities() + more).c_str(), console, &arena(), true) {
    CHECK_MSG(fx.loaded, fx.error);
  }
  int fd(const char* name) {
    const int f = fx.field(name);
    CHECK_MSG(f >= 0, name);
    return f;
  }
  float now() const { return static_cast<float>(fx.s->sv.time); }
  float get(int e, const char* name, int k = 0) { return fx.f(e, fd(name), k); }
  void set(int e, const char* name, float v, int k = 0) {
    fx.in_call([&] { fx.set_f(e, fd(name), v, k); });
  }
  void set_ent(int e, const char* name, int other) {
    fx.in_call([&] { *pg::entity_field(fx.vm(), e, fd(name)) = pg::entity_value(fx.vm(), other); });
  }
  void set_str(int e, const char* name, const char* text) {
    fx.in_call([&] { *pg::entity_field(fx.vm(), e, fd(name)) = pg::engine_string(fx.vm(), text); });
  }
  uint32_t sequence = 0;
  int joined = 0;
  void frames(int n) {
    for (int i = 0; i < n; i++) {
      sh::Packet pk[4];
      sequence++;
      for (int c = 0; c < joined; c++) {
        pk[c].client = c;
        pk[c].sequence = sequence;
        for (sh::MoveCmd& m : pk[c].cmds) {
          m = {};
          m.msec = 14;
        }
      }
      sv::qc_frame(*fx.s, pk, joined);
    }
  }
  int join(const char* name, bool spectator = false) { return fx.join(joined++, name, spectator); }
  // `name` run as the program's own call with `self` and `other`.
  void run(const char* name, int self, int other = 0) {
    sv::QcServer& q = *fx.s;
    q.sv.g.self = self;
    q.sv.g.other = other;
    q.sv.g.time = static_cast<float>(q.sv.time);
    shooters_edge::sim::enter_qc(q.q, q.sv);
    const int fn = pg::find_function(fx.progs, name);
    CHECK_MSG(fn > 0, name);
    if (fn > 0) pg::call_function(q.q.vm, fn);
    shooters_edge::sim::leave_qc(q.q);
  }
  void put(int p, sh::Vec3f at) {
    fx.call(2, {ent(p), at});
    for (int k = 0; k < 3; k++) set(p, "velocity", 0, k);
  }
  void give(int p, float bit, const char* until_field, float seconds) {
    set(p, until_field, now() + seconds);
    const int items = static_cast<int>(get(p, "items"));
    set(p, "items", static_cast<float>(items | static_cast<int>(bit)));
  }
  int first(const char* classname, int after = 0) {
    for (int e = after + 1; e < fx.s->sv.num_edicts; e++)
      if (!fx.s->sv.edicts[static_cast<size_t>(e)].free &&
          fx.str_field(e, fd("classname")) == classname)
        return e;
    return 0;
  }
  // Every print this frame, in order, then forgotten.
  std::string said() {
    std::string out;
    for (const sv::NetEvent& e : fx.s->out.events)
      if (e.kind == sv::EventKind::print) out += e.text;
    fx.s->out.events.clear();
    return out;
  }
  // `p` crushed by the door, which nobody opened: a death by the world.
  void crush(int p) {
    const int door = first("func_door");
    CHECK(door > 0);
    set(door, "dmg", 500);
    run("door_blocked", door, p);
  }
  void no_fault() { CHECK_MSG(fx.s->q.faults == 0, fx.s->q.last_fault); }
};

bool has(const std::string& s, const std::string& what) {
  return s.find(what) != std::string::npos;
}

}  // namespace

TEST(a_door_crush_is_credited_to_whoever_opened_it) {
  for (const bool team : {false, true}) {
    Rig r(quake({"deathmatch 1", team ? "teamplay 1" : "teamplay 0"}));
    const int a = r.join("a");
    const int b = r.join("b");
    if (team) {
      sv::set_user_info(*r.fx.s, 0, "team", "red");
      sv::set_user_info(*r.fx.s, 1, "team", "red");
    }
    r.frames(3);
    const int door = r.first("func_door");
    CHECK(door > 0);
    r.set_ent(door, "goalentity", b);
    r.set(door, "dmg", 500);
    r.said();
    r.run("door_blocked", door, a);
    const std::string out = r.said();
    if (team) {
      CHECK_MSG(has(out, "b squished a teammate\n"), out);
      CHECK(r.get(b, "frags") == -1);
    } else {
      CHECK_MSG(has(out, "b squishes a\n"), out);
      CHECK(r.get(b, "frags") == 1);
    }
    r.no_fault();
  }
  // The arena's own rules: the door's, as before.
  Rig r(quake({"deathmatch 1"}, false));
  const int a = r.join("a");
  const int b = r.join("b");
  r.frames(3);
  const int door = r.first("func_door");
  r.set_ent(door, "goalentity", b);
  r.set(door, "dmg", 500);
  r.run("door_blocked", door, a);
  CHECK(r.get(b, "frags") == 0);
  r.no_fault();
}

TEST(the_pentagram_deflects_a_telefrag_and_two_both_die) {
  for (const bool both : {false, true}) {
    Rig r(quake({"deathmatch 1"}));
    const int a = r.join("a");
    const int b = r.join("b");
    r.frames(3);
    r.put(b, {0, -256, 24});
    r.give(b, kItPent, "invincible_finished", 30);
    if (both) r.give(a, kItPent, "invincible_finished", 30);
    const int tele = r.fx.spawn();
    r.set_str(tele, "target", "d");
    r.said();
    r.run("teleport_touch", tele, a);
    const std::string out = r.said();
    if (!both) {
      CHECK_MSG(has(out, "Satan's power deflects a's telefrag\n"), out);
      CHECK(r.get(a, "health") <= 0);
      CHECK(r.get(b, "health") > 0);
      CHECK(r.get(a, "frags") == -1);
      CHECK(r.get(b, "frags") == 0);
    } else {
      CHECK_MSG(has(out, "a was telefragged by b's Satan's power\n"), out);
      CHECK_MSG(has(out, "b was telefragged by a's Satan's power\n"), out);
      CHECK(r.get(a, "health") <= 0);
      CHECK(r.get(b, "health") <= 0);
      CHECK(r.get(a, "invincible_finished") == 0);
    }
    r.no_fault();
  }
}

TEST(dq_and_dr_drop_the_quad_and_ring_and_everyone_hears_who_takes_them) {
  for (const bool keys : {false, true}) {
    std::vector<std::string> extra = {"deathmatch 1"};
    if (keys) {
      extra.emplace_back("localinfo dq 1");
      extra.emplace_back("localinfo dr 1");
    }
    Rig r(quake(extra));
    const int a = r.join("a");
    const int b = r.join("b");
    r.frames(3);
    r.give(a, kItQuad, "super_damage_finished", 20);
    r.give(a, kItRing, "invisible_finished", 10);
    r.said();
    r.crush(a);
    const std::string out = r.said();
    if (!keys) {
      CHECK_MSG(!has(out, " lost a "), out);
      const int placed = r.first("item_artifact_super_damage");
      CHECK(r.first("item_artifact_super_damage", placed) == 0);
      r.no_fault();
      continue;
    }
    CHECK_MSG(has(out, "a lost a quad with 20 seconds remaining\n"), out);
    CHECK_MSG(has(out, "a lost a ring with 10 seconds remaining\n"), out);
    const int quad = r.first("item_artifact_super_damage", r.first("item_artifact_super_damage"));
    CHECK(quad > 0);
    r.frames(72);  // a second on: 19 left
    r.run("dropped_power_touch", quad, b);
    const std::string took = r.said();
    CHECK_MSG(has(took, "b recovered a Quad with 19 seconds remaining!\n"), took);
    CHECK((static_cast<int>(r.get(b, "items")) & static_cast<int>(kItQuad)) != 0);
    r.no_fault();
  }
}

TEST(ammunition_brings_up_the_best_weapon_for_one_holding_the_best) {
  for (const bool rules : {false, true}) {
    Rig r(quake({"deathmatch 1"}, rules));
    const int a = r.join("a");
    r.frames(3);
    const int items = static_cast<int>(r.get(a, "items"));
    r.set(a, "items", static_cast<float>(items | static_cast<int>(kItNailgun)));
    r.set(a, "ammo_nails", 0);
    r.set(a, "weapon", kItShotgun);
    const int spikes = r.first("item_spikes");
    CHECK(spikes > 0);
    r.run("ammo_touch", spikes, a);
    CHECK(r.get(a, "ammo_nails") > 0);
    CHECK_MSG(r.get(a, "weapon") == (rules ? kItNailgun : kItShotgun),
              std::to_string(r.get(a, "weapon")));
    r.no_fault();
  }
}

TEST(the_old_ammunition_box_gives_its_kind_and_big_boxes_lie_as_the_big_model) {
  const std::string boxes =
      "{\n\"classname\" \"item_weapon\"\n\"spawnflags\" \"12\"\n\"origin\" \"64 -96 24\"\n}\n"
      "{\n\"classname\" \"item_weapon\"\n\"spawnflags\" \"3\"\n\"origin\" \"-160 96 24\"\n}\n"
      "{\n\"classname\" \"item_weapon\"\n\"origin\" \"-160 -96 24\"\n}\n"
      "{\n\"classname\" \"item_shells\"\n\"spawnflags\" \"1\"\n\"origin\" \"-64 -96 24\"\n}\n";
  for (const float dm : {1.0f, 4.0f}) {
    Rig r(quake({dm == 4 ? "deathmatch 4" : "deathmatch 1"}), boxes);
    const int a = r.join("a");
    r.frames(30);
    const int nails = r.first("item_weapon");
    const int rockets = r.first("item_weapon", nails);
    const int none = r.first("item_weapon", rockets);
    CHECK(nails > 0 && rockets > 0 && none > 0);  // it lies in deathmatch 4 too
    CHECK(r.fx.str_field(nails, r.fd("model")) == "maps/b_nail1.bsp");
    CHECK(r.fx.str_field(rockets, r.fd("model")) == "maps/b_rock0.bsp");  // rockets win
    CHECK(r.fx.str_field(none, r.fd("model")).empty());
    const int big = r.first("item_shells");
    if (dm == 4) {
      CHECK(big == 0);  // the other boxes do not lie in deathmatch 4
    } else {
      CHECK(big > 0);
      if (big > 0) CHECK(r.fx.str_field(big, r.fd("model")) == "maps/b_shell1.bsp");
    }
    for (const char* ammo : {"ammo_shells", "ammo_nails", "ammo_rockets", "ammo_cells"})
      r.set(a, ammo, 0);  // deathmatch 4 hands out more than a box may top up
    r.said();
    r.run("ammo_touch", nails, a);
    CHECK(r.get(a, "ammo_nails") == 40);
    r.run("ammo_touch", rockets, a);
    CHECK(r.get(a, "ammo_rockets") == 5);
    r.run("ammo_touch", none, a);
    const std::string out = r.said();
    CHECK_MSG(out == "You got the spikes\nYou got the rockets\nYou got the \n", out);
    r.no_fault();
  }
}

// Classic remix's other look (`cr_items`) dresses the old box as that game's box of its kind,
// as it does item_shells and the rest; a box of no kind keeps no model.
TEST(the_old_ammunition_box_wears_the_remixs_box_of_its_kind) {
  const std::string boxes =
      "{\n\"classname\" \"item_weapon\"\n\"spawnflags\" \"4\"\n\"origin\" \"64 -96 24\"\n}\n"
      "{\n\"classname\" \"item_weapon\"\n\"spawnflags\" \"1\"\n\"origin\" \"-160 96 24\"\n}\n"
      "{\n\"classname\" \"item_weapon\"\n\"origin\" \"-160 -96 24\"\n}\n";
  Rig r(
      quake(
          {"deathmatch 1",
           "set arsenal_ammo1 \"models/items/ammo/shells/medium/tris.md2 misc/am_pkup.wav 15\"",
           "set arsenal_ammo2 \"models/items/ammo/bullets/medium/tris.md2 misc/am_pkup.wav 15\""}),
      boxes);
  r.join("a");
  r.frames(3);
  const int nails = r.first("item_weapon");
  const int shells = r.first("item_weapon", nails);
  const int none = r.first("item_weapon", shells);
  CHECK(nails > 0 && shells > 0 && none > 0);
  CHECK_MSG(r.fx.str_field(nails, r.fd("model")) == "models/items/ammo/bullets/medium/tris.md2",
            r.fx.str_field(nails, r.fd("model")));
  CHECK_MSG(r.fx.str_field(shells, r.fd("model")) == "models/items/ammo/shells/medium/tris.md2",
            r.fx.str_field(shells, r.fd("model")));
  CHECK(r.fx.str_field(none, r.fd("model")).empty());
  r.no_fault();
}

TEST(the_octapower_says_its_own_name_wearing_off) {
  Rig r(quake({"deathmatch 4"}));
  const int a = r.join("a");
  r.frames(3);
  r.give(a, kItQuad, "super_damage_finished", 2.5f);
  r.said();
  r.frames(3);
  const std::string out = r.said();
  CHECK_MSG(has(out, "OctaPower is wearing off\n"), out);
  CHECK_MSG(!has(out, "Quad Damage"), out);
  r.no_fault();
}

TEST(a_spectators_impulse_1_visits_each_deathmatch_spot) {
  Rig r(quake({"deathmatch 1"}));
  r.join("a");
  const int s = r.join("watcher", true);
  r.frames(3);
  std::vector<float> xs;
  for (int i = 0; i < 5; i++) {
    r.set(s, "impulse", 1);
    r.run("SpectatorThink", s);
    CHECK(r.get(s, "impulse") == 0);
    const sh::Edict& e = r.fx.s->sv.edicts[static_cast<size_t>(s)];
    CHECK(e.origin.z == 24);
    xs.push_back(e.origin.x + e.origin.y * 1000);
  }
  // Four spots in turn, then the first again.
  CHECK(xs[0] != xs[1] && xs[1] != xs[2] && xs[2] != xs[3]);
  CHECK(xs[4] == xs[0]);
  r.no_fault();
}

TEST(a_named_light_switches_and_the_fluorescent_ones_hum) {
  Rig r(quake({"deathmatch 1"}));
  const int sw = r.first("light");
  int named = 0;
  for (int e = sw; e > 0 && e < r.fx.s->sv.num_edicts; e++)
    if (r.fx.str_field(e, r.fd("targetname")) == "sw") named = e;
  CHECK_MSG(named > 0, "the named light was removed");
  CHECK(r.first("light_fluoro") > 0);
  r.join("a");
  r.frames(2);
  r.fx.s->out.events.clear();
  r.run("light_use", named, 1);
  auto styles = r.fx.events(sv::EventKind::lightstyle);
  CHECK(styles.size() == 1u);
  if (styles.size() == 1u) CHECK(styles[0]->level == 33 && styles[0]->text == "m");
  r.fx.s->out.events.clear();
  r.run("light_use", named, 1);
  styles = r.fx.events(sv::EventKind::lightstyle);
  if (styles.size() == 1u) CHECK(styles[0]->text == "a");
  // The hum and the buzz are the map's ambient sounds.
  bool hum = false, buzz = false;
  for (const std::string& snd : r.fx.s->world.sounds) {
    hum = hum || snd == "ambience/fl_hum1.wav";
    buzz = buzz || snd == "ambience/buzz1.wav";
  }
  CHECK(hum && buzz);
  r.no_fault();
  // The arena's own rules keep no light.
  Rig own(quake({"deathmatch 1"}, false));
  CHECK(own.first("light") == 0 && own.first("light_fluoro") == 0);
}

TEST(a_body_on_the_ground_slows_by_20_each_command) {
  Rig r(quake({"deathmatch 1"}));
  const int a = r.join("a");
  r.frames(3);
  r.crush(a);
  CHECK(r.get(a, "deadflag") != 0);
  r.set(a, "velocity", 100, 0);
  r.set(a, "velocity", 0, 1);
  r.set(a, "velocity", 0, 2);
  r.set(a, "flags",
        static_cast<float>(static_cast<int>(r.get(a, "flags")) | static_cast<int>(kFlOnground)));
  r.run("PlayerPreThink", a);
  CHECK_MSG(r.get(a, "velocity") == 80, std::to_string(r.get(a, "velocity")));
  r.set(a, "velocity", 15, 0);
  r.run("PlayerPreThink", a);
  CHECK(r.get(a, "velocity") == 0);
  r.no_fault();
}

TEST(the_operators_say_and_the_pause_lines_are_quakeworlds) {
  Rig r(quake({"deathmatch 1"}));
  r.join("a");
  r.frames(2);
  r.said();
  CHECK(sv::server_command(*r.fx.s, "say hello all"));
  std::vector<const sv::NetEvent*> p = r.fx.events(sv::EventKind::print);
  CHECK(p.size() == 1u);
  if (p.size() == 1u) {
    CHECK_MSG(p[0]->text == "console: hello all\n", p[0]->text);
    CHECK(p[0]->level == 3);
  }
  r.said();
  // QuakeWorld words the pause by the state before it: a pause reads "unpaused".
  sv::client_command(*r.fx.s, 0, "pause");
  CHECK(r.fx.s->paused);
  CHECK_MSG(r.said() == "a unpaused the game\n", "pause");
  // One who joins a paused game is told.
  sv::connect(*r.fx.s, 1, "b", false, false);
  const std::vector<const sv::NetEvent*> told = r.fx.events(sv::EventKind::print);
  bool paused_told = false;
  for (const sv::NetEvent* e : told)
    paused_told = paused_told || (e->text == "Server is paused.\n" && e->to == 2u);
  CHECK(paused_told);
  CHECK(!r.fx.events(sv::EventKind::pause).empty());
  r.said();
  // Everyone gone: the server lets go of the pause and says why.
  sv::disconnect(*r.fx.s, 1);
  sv::disconnect(*r.fx.s, 0);
  CHECK(!r.fx.s->paused);
  bool released = false;
  for (const std::string& l : r.fx.s->console)
    released = released || l == "Pause released since no players are left.";
  CHECK(released);
  r.no_fault();
}

TEST(a_dead_player_or_a_spectator_cannot_suicide) {
  Rig r(quake({"deathmatch 1"}));
  const int a = r.join("a");
  r.join("watcher", true);
  r.frames(2);
  r.said();
  sv::client_command(*r.fx.s, 1, "kill");
  CHECK(r.said() == "Can't suicide -- allready dead!\n");
  r.fx.s->sv.edicts[static_cast<size_t>(a)].health = -5;
  sv::client_command(*r.fx.s, 0, "kill");
  CHECK(r.said() == "Can't suicide -- allready dead!\n");
  r.no_fault();
}

TEST(a_trigger_with_an_angle_fires_only_for_one_facing_its_way) {
  Rig r(quake({"deathmatch 1"}));
  const int a = r.join("a");
  r.frames(2);
  const int t = r.fx.spawn();
  r.set_str(t, "classname", "trigger_multiple");
  r.set_str(t, "target", "sw");
  r.set(t, "wait", 0.2f);
  r.set(t, "movedir", 1, 0);  // its way: +x
  for (const float yaw : {180.0f, 0.0f}) {
    r.set(a, "angles", yaw, 1);
    r.fx.s->out.events.clear();
    r.run("trigger_touch", t, a);
    const size_t fired = r.fx.events(sv::EventKind::lightstyle).size();
    CHECK_MSG(fired == (yaw == 0 ? 1u : 0u), std::to_string(yaw));
  }
  r.no_fault();
}

TEST(a_trigger_with_health_is_fired_by_a_blow_and_comes_back) {
  Rig r(quake({"deathmatch 1"}));
  r.join("a");
  r.frames(2);
  const int t = r.first("trigger_multiple");
  CHECK(t > 0);
  CHECK(r.get(t, "takedamage") == 1 && r.get(t, "solid") == 2 && r.get(t, "max_health") == 10);
  r.fx.s->out.events.clear();
  r.run("trigger_killed", t);
  CHECK(r.fx.events(sv::EventKind::lightstyle).size() == 1u);
  CHECK(r.get(t, "takedamage") == 0);
  r.frames(72 * 2);  // past its wait
  CHECK(r.get(t, "takedamage") == 1 && r.get(t, "health") == 10 && r.get(t, "solid") == 2);
  r.no_fault();
  // The arena's own rules: a trigger, not a target.
  Rig own(quake({"deathmatch 1"}, false));
  const int o = own.first("trigger_multiple");
  CHECK(o > 0 && own.get(o, "takedamage") == 0);
}

TEST(a_rune_opens_the_episode_and_the_gates_and_the_full_games_trigger_answer) {
  Rig r(quake({"deathmatch 1"}));
  const int a = r.join("a");
  r.frames(2);
  // No episode done: the episode's gate is gone and the boss's stands.
  CHECK(r.first("func_episodegate") == 0);
  CHECK(r.first("func_bossgate") > 0);
  const int rune = r.first("item_sigil");
  CHECK(rune > 0);
  r.said();
  r.fx.s->out.events.clear();
  r.run("sigil_touch", rune, a);
  bool told = false, flashed = false;
  for (const sv::NetEvent& e : r.fx.s->out.events) {
    told = told || (e.kind == sv::EventKind::centerprint && e.text == "You got the rune!");
    flashed = flashed || (e.kind == sv::EventKind::stuffcmd && e.text == "bf\n");
  }
  CHECK(told);
  CHECK(flashed);
  CHECK(r.first("item_sigil") == 0);
  const int flags = r.fx.s->q.layout->g.serverflags;
  CHECK(flags >= 0);
  if (flags >= 0) CHECK(pg::global_float(r.fx.s->q.vm, flags) == 2);
  // The full game's trigger on the shareware packs: its words, at most once in 2 s.
  const int only = r.first("trigger_onlyregistered");
  CHECK(only > 0);
  r.fx.s->out.events.clear();
  r.run("onlyregistered_touch", only, a);
  r.run("onlyregistered_touch", only, a);
  int words = 0;
  for (const sv::NetEvent& e : r.fx.s->out.events)
    words += e.kind == sv::EventKind::centerprint && e.text == "Not in this version" ? 1 : 0;
  CHECK(words == 1);
  // The teleport ball spins on its path, not solid.
  const int ball = r.first("misc_teleporttrain");
  CHECK(ball > 0);
  r.frames(20);
  const sh::Edict& b = r.fx.s->sv.edicts[static_cast<size_t>(ball)];
  CHECK(b.solid == 0);
  CHECK(r.get(ball, "avelocity", 1) == 200);
  CHECK(r.get(ball, "velocity") > 0);
  r.no_fault();
}

TEST(the_arenas_own_rules_have_no_rune) {
  Rig r(quake({"deathmatch 1"}, false));
  r.frames(1);
  CHECK(r.first("item_sigil") == 0);
  r.no_fault();
}
