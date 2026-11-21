// LAB `se-coop-sweep`: does co-op hold together on every map of the campaign? Two players in a
// coop game of the pack's own program (`--game`, with `--progs=pack`; our campaign otherwise) on
// each map in turn, put through what a co-op evening does to a map, with no bot finding its way:
//  1. both join a new game on the map and stand 2 s: both alive, nothing faulted;
//  2. the second player dies to the world's blow (the program's own damage) and presses attack
//     until the player stands again;
//  3. that player leaves and joins again;
//  4. each key (item_key*) taken by the first player, then by the second where it lay;
//  5. on the last map, its last foe struck down as the telefrag does: the ending for both;
//  6. the first is put inside the map's exit (each trigger_changelevel in turn, the first that
//     changes the map) and both press attack through the intermission: the next map is the exit's,
//     both players are on it, alive, with the weapons they carried.
// The table, a map a row: each step's outcome, the next map, the program's faults and the last
// one's text. Without `--game` (the smoke run) the coop hall with its exit, twice over.
//
// ge:summary Lab: co-op on every campaign map: joins, a death, a leave and rejoin, the exit
// ge:keywords lab se-coop-sweep coop sweep every-map join leave rejoin kill respawn exit
// ge:keywords changelevel intermission carried-parms real-maps keys finale
// ge:skill shooter labs
// ge:test tests/shooters_edge_campaign_coop_test.cpp
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "classicenhance/source.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "progs/file.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/bots/coop_run.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/sim/qc_program.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace fb = shooters_edge::bots;
namespace ce = ge::classicenhance;

constexpr int kMsec = 14;  // a command on the 1/72 s host
constexpr int kSecond = 72;

const char* const kEveryMap =
    "start,e1m1,e1m2,e1m3,e1m4,e1m5,e1m6,e1m7,e1m8,e2m1,e2m2,e2m3,e2m4,e2m5,e2m6,e2m7,e3m1,e3m2,"
    "e3m3,e3m4,e3m5,e3m6,e3m7,e4m1,e4m2,e4m3,e4m4,e4m5,e4m6,e4m7,e4m8,end";

struct PackMaps {
  std::vector<uint8_t> content;
  ge::vfs::Pak pak;
  std::unique_ptr<sv::BspMap> bsp;
  pg::Progs progs;
};

bool pack_map(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  auto& m = *static_cast<PackMaps*>(ctx);
  const ge::vfs::PakEntry* e = ge::vfs::pak_find(m.pak, ("maps/" + name + ".bsp").c_str());
  if (e == nullptr) {
    error = "no maps/" + name + ".bsp in the packs";
    return false;
  }
  m.bsp = std::make_unique<sv::BspMap>();
  return sv::read_bsp_map(ge::vfs::pak_file(m.content, *e), name, m.bsp.get(), &out, error);
}

struct Sweep {
  sv::Campaign c;
  uint32_t seq[2] = {};
  std::vector<sh::Packet> packets;

  sv::QcServer* qs() { return c.server ? sv::qc_of(c.server) : nullptr; }
  bool active(int slot) { return c.server && c.server->clients[slot].active; }
  float field(int e, int f) {
    sv::QcServer* s = qs();
    return s == nullptr ? 0 : pg::as_float(*pg::entity_field(s->q.vm, e, f));
  }
  float health(int slot) { return field(slot + 1, qs()->q.layout->f.health); }
  float items(int slot) { return field(slot + 1, qs()->q.layout->f.items); }

  // `frames` frames, each active player sending a still command (attack on odd frames when
  // `attack`); false when a change failed.
  bool run(int frames, bool attack, std::string& error) {
    for (int f = 0; f < frames; f++) {
      packets.clear();
      for (int k = 0; k < 2; k++) {
        if (!active(k)) continue;
        sh::Packet p;
        p.client = k;
        p.sequence = ++seq[k];
        for (sh::MoveCmd& m : p.cmds) m.msec = kMsec;
        if (attack && (f & 1) != 0) p.cmds[2].buttons = sh::kButtonAttack;
        packets.push_back(p);
      }
      if (!sv::campaign_frame(c, packets.data(), static_cast<int>(packets.size()), error))
        return false;
    }
    return true;
  }
};

struct Row {
  std::string map, start = "-", death = "-", rejoin = "-", keys = "-", finale = "-", exit = "-",
                   next = "-", carried = "-";
  int64_t faults = 0;
  std::string fault;
};

std::string class_of(sv::QcServer& s, int e) {
  const char* cls = pg::vm_string(
      s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, s.q.layout->f.classname)));
  return cls == nullptr ? "" : cls;
}

// Every live entity whose class starts with `prefix`, by number.
std::vector<int> of_class(sv::QcServer& s, const std::string& prefix) {
  std::vector<int> out;
  for (int e = 1; e < s.sv.num_edicts; e++)
    if (!s.sv.edicts[static_cast<size_t>(e)].free && class_of(s, e).rfind(prefix, 0) == 0)
      out.push_back(e);
  return out;
}

// Player entity `player` put at `at` and linked, its triggers touched there.
void put(sv::QcServer& s, int player, sh::Vec3f at) {
  s.sv.edicts[static_cast<size_t>(player)].origin = at;
  sh::link_edict(s.sv, player, true);
}

// The map's exits: each trigger_changelevel's box middle and the map it names.
std::vector<std::pair<sh::Vec3f, std::string>> exits(sv::QcServer& s) {
  std::vector<std::pair<sh::Vec3f, std::string>> out;
  const pg::Def* map_field = pg::find_field(*s.q.vm.progs, "map");
  for (int e = 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    if (ed.free || class_of(s, e) != "trigger_changelevel") continue;
    std::string next;
    if (map_field != nullptr) {
      const char* m = pg::vm_string(
          s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, map_field->offset)));
      next = m == nullptr ? "" : m;
    }
    out.push_back({(ed.absmin + ed.absmax) * 0.5f, next});
  }
  return out;
}

// The program's own damage (T_Damage) on `victim` from the world, `amount` of it.
void hurt(sv::QcServer& s, int victim, float amount) {
  const int fn = pg::find_function(*s.q.vm.progs, "T_Damage");
  if (fn <= 0) return;
  shooters_edge::sim::enter_qc(s.q, s.sv);
  pg::Vm& vm = s.q.vm;
  pg::set_global_int(vm, pg::parm(0), pg::entity_value(vm, victim));
  pg::set_global_int(vm, pg::parm(1), pg::entity_value(vm, 0));
  pg::set_global_int(vm, pg::parm(2), pg::entity_value(vm, 0));
  pg::set_global_float(vm, pg::parm(3), amount);
  pg::set_global_int(vm, pg::vm_layout(vm).g.self, pg::entity_value(vm, 0));
  pg::set_global_int(vm, pg::vm_layout(vm).g.other, pg::entity_value(vm, 0));
  pg::call_function(vm, fn);
  pg::clear_fault(vm);
  shooters_edge::sim::leave_qc(s.q);
}

Row sweep_map(const pg::Progs& progs, sv::CampaignOptions co, const std::string& map) {
  Row r;
  r.map = map;
  Sweep w;
  std::string error;
  if (!sv::campaign_new_game(w.c, progs, co, map, error)) {
    r.start = "no map: " + error;
    return r;
  }
  sv::campaign_join(w.c, 0, "one");
  sv::campaign_join(w.c, 1, "two");
  const auto done = [&](Row& row) {
    if (sv::QcServer* s = w.qs()) {
      row.faults = s->q.faults;
      row.fault = s->q.last_fault;
    }
    return row;
  };
  if (!w.run(2 * kSecond, false, error)) {
    r.start = "changed: " + error;
    return done(r);
  }
  r.start =
      w.active(0) && w.active(1) && w.health(0) > 0 && w.health(1) > 0 ? "ok" : "a player down";
  if (w.c.history.size() > 1) {  // the map moved on by itself (a cutscene, the end)
    r.start = "moved on to " + w.c.history.back();
    return done(r);
  }

  // A death (the world's blow, as a fall or a crusher deals it) and the respawn.
  // A program without the damage call (our own campaign) dies by the kill command instead.
  const bool damage = w.qs() != nullptr && pg::find_function(*w.qs()->q.vm.progs, "T_Damage") > 0;
  if (damage)
    hurt(*w.qs(), 2, 1000);
  else if (w.c.server)
    w.c.server.client_command(1, "kill");
  w.run(kSecond, false, error);
  if (!damage && w.health(1) > 0) {
    r.death = "up in 0.0 s";
  } else if (w.health(1) > 0) {
    r.death = "did not die";
  } else {
    int f = 0;
    for (; f < 5 * kSecond && w.health(1) <= 0; f += 6) w.run(6, true, error);
    r.death =
        w.health(1) > 0 ? "up in " + ge::core::num::to_fixed(f / 72.0, 1) + " s" : "not up in 5 s";
  }

  // A leave and a rejoin.
  w.c.players[1].active = false;
  if (w.c.server && w.c.server->clients[1].active) w.c.server.drop(1);
  w.run(kSecond, false, error);
  sv::campaign_join(w.c, 1, "two");
  w.run(kSecond, false, error);
  r.rejoin = w.active(1) && w.health(1) > 0 ? "ok" : "not back";

  // The keys: each one taken by the first player, then by the second where it lay (a coop game
  // leaves a key for everyone).
  if (sv::QcServer* s = w.qs()) {
    const std::vector<int> keys = of_class(*s, "item_key");
    int both = 0;
    for (const int k : keys) {
      sv::QcServer* q = w.qs();
      if (q == nullptr) break;
      const sh::Edict& ed = q->sv.edicts[static_cast<size_t>(k)];
      const sh::Vec3f at = ed.origin + (ed.mins + ed.maxs) * 0.5f;
      const float had[2] = {w.items(0), w.items(1)};
      put(*q, 1, at);
      w.run(kSecond / 2, false, error);
      if ((q = w.qs()) == nullptr) break;
      put(*q, 2, at);
      w.run(kSecond / 2, false, error);
      if (w.items(0) != had[0] && w.items(1) != had[1]) both++;
    }
    if (!keys.empty())
      r.keys = std::to_string(both) + " of " + std::to_string(keys.size()) + " by both";
  }

  // The finale: the map's last foe (monster_oldone) struck down as the telefrag does, then the
  // ending's intermission for both.
  if (sv::QcServer* s = w.qs(); s != nullptr && !of_class(*s, "monster_oldone").empty()) {
    const pg::Def* inter = pg::find_global(*s->q.vm.progs, "intermission_running");
    hurt(*s, of_class(*s, "monster_oldone").front(), 50000);
    int f = 0;
    const auto running = [&] {
      sv::QcServer* q = w.qs();
      return q != nullptr && inter != nullptr && pg::global_float(q->q.vm, inter->offset) != 0;
    };
    for (; f < 30 * kSecond && !running(); f += 6) w.run(6, false, error);
    if (running()) w.run(20 * kSecond, false, error);  // the ending's scenes play out
    r.finale = running() && w.active(0) && w.active(1)
                   ? "the end in " + ge::core::num::to_fixed(f / 72.0, 1) + " s"
                   : "no ending";
    if (running()) return done(r);
  }

  // The exit.
  sv::QcServer* s = w.qs();
  if (s == nullptr) return done(r);
  const auto ways = exits(*s);
  if (ways.empty()) {
    r.exit = "no exit trigger";
    return done(r);
  }
  // The keys stay on the map they open: a level change takes them from everyone.
  constexpr int kKeys = 131072 | 262144;
  const auto kept = [&](int slot) { return static_cast<int>(w.items(slot)) & ~kKeys; };
  const int carried = kept(0);
  const size_t before = w.c.history.size();
  for (const auto& [at, next] : ways) {
    if (sv::QcServer* q = w.qs()) {
      sh::Edict& p = q->sv.edicts[1];
      p.origin = at;
      sh::link_edict(q->sv, 1, true);
    }
    int f = 0;
    for (; f < 20 * kSecond && w.c.history.size() == before; f += 6) {
      if (!w.run(6, f > 3 * kSecond, error)) {
        r.exit = "the next map failed: " + error;
        return r;
      }
    }
    if (w.c.history.size() != before) {
      r.exit = "out in " + ge::core::num::to_fixed(f / 72.0, 1) + " s";
      break;
    }
    r.exit = "never left";
  }
  if (w.c.history.size() == before) return done(r);
  r.next = w.c.history.back();
  w.run(kSecond, false, error);
  const bool both = w.active(0) && w.active(1) && w.health(0) > 0 && w.health(1) > 0;
  r.carried = !both                ? "a player missing"
              : kept(0) == carried ? "both, kept"
                                   : "both, items changed";
  return done(r);
}

// "n of m by both" with n == m.
bool keys_all(const std::string& keys) {
  const size_t of = keys.find(" of ");
  return of != std::string::npos &&
         keys.substr(0, of) == keys.substr(of + 4, keys.find(" by") - of - 4);
}

void run(gl::Lab& lab) {
  PackMaps packs;
  sv::CampaignOptions co;
  co.server.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  co.server.loop.max_edicts = 1024;
  co.settings = {"deathmatch 0", "coop 1", "skill " + lab.arg("skill")};
  pg::Progs own;
  const pg::Progs* progs = &own;
  std::vector<std::string> maps;
  const std::string& game = lab.arg("game");
  std::string error;
  if (game.empty()) {
    co.load = &fb::coop_hall_exit_map;
    maps = {"hall", "hall"};
  } else {
    ce::Source src;
    if (!ce::open_source(game, &src, &error)) {
      lab.fail("se-coop-sweep: " + error, "next: pass --game=<a release zip, a pack or a folder>");
      return;
    }
    packs.content = src.content();
    if (!ge::vfs::read_pak(packs.content, &packs.pak, &error)) {
      lab.fail("se-coop-sweep: the pack does not read: " + error, "next: check the --game path");
      return;
    }
    co.load = &pack_map;
    co.load_ctx = &packs;
    co.registered = ge::vfs::pak_find(packs.pak, "gfx/pop.lmp") != nullptr;
    co.wide_models = true;
    std::stringstream in(lab.arg("maps").empty() ? kEveryMap : lab.arg("maps"));
    for (std::string m; std::getline(in, m, ',');)
      if (ge::vfs::pak_find(packs.pak, ("maps/" + m + ".bsp").c_str()) != nullptr)
        maps.push_back(m);
    if (lab.arg("progs") == "pack") {
      const ge::vfs::PakEntry* e = ge::vfs::pak_find(packs.pak, "progs.dat");
      const std::span<const uint8_t> bytes =
          e != nullptr ? ge::vfs::pak_file(packs.content, *e) : std::span<const uint8_t>{};
      if (e == nullptr ||
          !pg::load_progs(bytes.data(), bytes.size(), packs.progs, error, pg::every_form())) {
        lab.fail("se-coop-sweep: the pack's program does not load: " + error,
                 "next: pass the game's own folder, or drop --progs");
        return;
      }
      progs = &packs.progs;
    }
  }
  if (progs == &own && !sv::campaign_mod(own, error)) {
    lab.fail("se-coop-sweep: the campaign: " + error,
             "next: run tests/shooters_edge_campaign_mod_test");
    return;
  }

  gl::LabTable t{"each map in co-op",
                 {"map", "both in", "a death", "leave, rejoin", "keys", "the finale", "the exit",
                  "next", "on the next map", "faults", "last fault"},
                 {}};
  int ok = 0, faults = 0;
  for (const std::string& m : maps) {
    const Row r = sweep_map(*progs, co, m);
    const bool good = r.start == "ok" && r.death.rfind("up in", 0) == 0 && r.rejoin == "ok" &&
                      (r.keys == "-" || keys_all(r.keys)) &&
                      (r.finale == "-" || r.finale.rfind("the end", 0) == 0) &&
                      (r.exit.rfind("out in", 0) == 0 || r.exit == "no exit trigger" ||
                       r.finale.rfind("the end", 0) == 0) &&
                      (r.next == "-" || r.carried.rfind("both", 0) == 0) && r.faults == 0;
    ok += good ? 1 : 0;
    faults += static_cast<int>(r.faults);
    std::string fault = r.fault.substr(0, r.fault.find('\n'));
    t.rows.push_back({r.map, r.start, r.death, r.rejoin, r.keys, r.finale, r.exit, r.next,
                      r.carried, std::to_string(r.faults), fault});
  }
  lab.table(t);
  lab.metric("maps", static_cast<double>(maps.size()), "count");
  lab.metric("maps.ok", ok, "count");
  lab.metric("faults", faults, "count");
}

const gl::LabSpec kSpec{
    "se-coop-sweep",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/server/campaign.hpp in coop: joins, a death, a leave and the exit on "
    "every map",
    "shooter",
    "Does co-op hold together on every map: both players in, a death and the respawn, a leave and "
    "a rejoin, the exit taken by both to the next map?",
    {
        {"game", "", "the player's own release zip, pack or folder (none: the coop hall twice)"},
        {"maps", "",
         "with --game: the maps, a comma list (none: every campaign map the packs hold)"},
        {"progs", "", "pack: the pack's own program (progs.dat) instead of our campaign"},
        {"skill", "1", "the campaign's skill (0..3)"},
        {"seed", "1", "the campaign's seed"},
    },
    "",
    run,
};

GE_LAB(kSpec);

}  // namespace
