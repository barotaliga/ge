// LAB `se-dive`: does a player in Quake's lava, water and slime take the classic damage, hear the
// classic sounds and swim the classic speeds? A hall with a pool of each (the plan below, compiled
// in the lab) is played on the campaign host by one player, by our own campaign and, with
// `--game`, by the pack's own program (progs.dat) too, so the game's own code is the record. In
// each pool, a scripted dive:
//  1. held at immersion level 1, then 2, then 3, two seconds each (put back each frame, still):
//     the health lost at each level, against what the engine's `step_harm` deals over the same
//     frames with the classic harm row (shooter/liquid_harm.hpp), and the sounds going in;
//  2. let go under the surface for a second: the sink;
//  3. put under again holding jump for half a second: the swim up;
//  4. out onto the floor (the sounds coming out), then back in for a second: the dip's damage;
//  5. in water alone, 14 s under: the drowning, and the sounds coming out after it.
// The table, a row a program and liquid; the verdict is the damage the row's to the point, and
// with `--game` the pack's sink, swim up, sounds (the burn's two cries as one, picked at random)
// and drowning those of our campaign. Without `--game` (the smoke run) our campaign alone.
//
// ge:summary Lab: a scripted dive into lava, water and slime: damage by level, sounds, swims
// ge:keywords lab se-dive dive lava slime damage tick swim-up sink waterlevel classic-harm
// ge:keywords step_harm pack-program campaign hazard-clock drowning gasp enter-liquid leave-water
// ge:skill shooter water labs
// ge:test tests/shooter_liquid_harm_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/plan.hpp"
#include "classicenhance/source.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "progs/file.hpp"
#include "progs/vm.hpp"
#include "shooter/liquid_harm.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "vfs/pak.hpp"
#include "water/harm.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace gw = ge::water;
namespace mc = ge::brush::mapc;
namespace ce = ge::classicenhance;
using ge::core::num::to_fixed;

constexpr int kMsec = 14;  // a command on the 1/72 s host
constexpr int kSecond = 72;
constexpr float kSurface = -24;  // both pools' surface, z

// The hall: a floor at 0 with three pools sunk in it, lava in the west, water in the middle and
// slime in the east, each 128 across and their liquid from 136 under the floor to 24 under it.
const char* const kPlan =
    "world message=\"The Dive\" light=200\n"
    "textures floor=floor01_5 wall=wbrick1_5 ceil=metal1_3 solid=tech04_1\n"
    "room hall z=0..192 box 0,0 640,256\n"
    "room lavapit z=-160..0 box 32,64 160,192\n"
    "liquid z=-160..-24 box 32,64 160,192 kind=lava tex=*lava1\n"
    "room waterpit z=-160..0 box 256,64 384,192\n"
    "liquid z=-160..-24 box 256,64 384,192 kind=water tex=*water1\n"
    "room slimepit z=-160..0 box 480,64 608,192\n"
    "liquid z=-160..-24 box 480,64 608,192 kind=slime tex=*slime0\n"
    "spawn 320,232,24 angle=0 class=info_player_start\n";

struct Hall {
  std::vector<uint8_t> bsp;
  std::unique_ptr<sv::BspMap> map;
};

bool hall_map(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  auto& h = *static_cast<Hall*>(ctx);
  h.map = std::make_unique<sv::BspMap>();
  return sv::read_bsp_map(h.bsp, name, h.map.get(), &out, error);
}

struct Dive {
  sv::Campaign c;
  uint32_t seq = 0;

  sv::QcServer& qs() { return *sv::qc_of(c.server); }
  sh::Edict& me() { return qs().sv.edicts[1]; }  // the server's own copy of the player

  std::string heard;  // the program's sounds on the player since the last `take`
  std::string take() { return std::exchange(heard, ""); }
  void frame(uint8_t buttons) {
    sh::Packet p;
    p.client = 0;
    p.sequence = ++seq;
    for (sh::MoveCmd& m : p.cmds) m.msec = kMsec;
    p.cmds[2].buttons = buttons;
    std::string error;
    sv::campaign_frame(c, &p, 1, error);
    if (!c.server) return;
    const sh::Server& w = qs().sv;
    for (int i = 0; i < w.sound_count; i++)
      if (w.sounds[i].ent == 1 && !w.sounds[i].engine)
        heard += (heard.empty() ? "" : " ") + std::string(w.sounds[i].sample);
  }
  // The player put with the origin at `z` over the pool's middle `at`, still.
  void put(sh::Vec3f at, float z) {
    sh::Edict& e = qs().sv.edicts[1];
    e.origin = {at.x, at.y, z};
    e.velocity = {};
    sh::link_edict(qs().sv, 1, true);
  }
};

struct Row {
  std::string program, liquid;
  double lost[3] = {}, want[3] = {};
  int levels[3] = {};  // the waterlevel each hold read
  double sink = 0, up = 0;
  double dip = 0, dip_want = 0;  // lost in a second back in after half a second out
  double drowned = 0;            // lost under water over 14 s (water alone)
  std::string in, out;           // the program's sounds going in and coming out
};

// The origin's height for immersion level 1, 2 and 3 in a pool whose surface is `kSurface`: the
// feet 8 under, the waist 6 under, the eyes 18 under.
constexpr sh::Vec3f kFloor{320, 232, 0};  // a spot on the hall's floor, out of every pool
constexpr float kAt[3] = {kSurface + 16, kSurface - 10, kSurface - 40};

Row dive_into(const pg::Progs& progs, const sv::CampaignOptions& co, const char* liquid,
              int contents, sh::Vec3f pool, std::string& error) {
  Row r;
  r.liquid = liquid;
  Dive d;
  if (!sv::campaign_new_game(d.c, progs, co, "dive", error)) return r;
  sv::campaign_join(d.c, 0, "diver");
  for (int i = 0; i < kSecond; i++) d.frame(0);
  // 1. The holds, two seconds at each level, the health kept high and counted.
  const gw::Harm harm = sh::classic_harm(contents);
  gw::HarmState st;
  // A frame of the host, and what the row deals over it at the level the program reads in it
  // (the one the last frame's physics left).
  const auto go = [&](uint8_t buttons) {
    const int seen = d.me().waterlevel;
    d.frame(buttons);
    return gw::step_harm(harm, 20, seen, st, 1.0 / kSecond);
  };
  d.take();
  for (int level = 1; level <= 3; level++) {
    for (int i = 0; i < 2 * kSecond; i++) {
      if (level == 1 && i == 4) r.in = d.take();
      if (d.me().health < 500) d.me().health = 1000;
      const float before = d.me().health;
      d.put(pool, kAt[level - 1]);
      const double row = go(0);
      r.lost[level - 1] += before - d.me().health;
      const int read = d.me().waterlevel;
      r.levels[level - 1] = std::max(r.levels[level - 1], read);
      r.want[level - 1] += row;
    }
  }
  // 2. The sink: let go under the surface for a second.
  d.me().health = 1000;
  d.put(pool, kAt[2] - 24);
  const float z0 = d.qs().sv.edicts[1].origin.z;
  for (int i = 0; i < kSecond; i++) {
    d.me().health = 1000;
    go(0);
  }
  r.sink = (d.qs().sv.edicts[1].origin.z - z0) / 1.0;
  // 3. The swim up: under again, jump held for half a second; the speed at its end.
  d.put(pool, kAt[2] - 24);
  for (int i = 0; i < kSecond / 2; i++) {
    d.me().health = 1000;
    go(sh::kButtonJump);
  }
  r.up = d.qs().sv.edicts[1].velocity.z;
  // 4. Out onto the floor for half a second, then back in at level 1 for a second.
  d.take();
  for (int i = 0; i < kSecond / 2; i++) {
    d.me().health = 1000;
    d.put(kFloor, 24);
    r.dip_want += go(0);
    if (i == 4) r.out = d.take();
  }
  for (int i = 0; i < kSecond; i++) {
    d.me().health = 1000;
    const float before = d.me().health;
    d.put(pool, kAt[0]);
    const double row = go(0);
    r.dip += before - d.me().health;
    r.dip_want += row;
  }
  // 5. Water alone: under for 14 s, the air run out.
  if (contents == sh::kContentsWater) {
    for (int i = 0; i < 14 * kSecond; i++) {
      d.me().health = std::max(d.me().health, 500.0f);
      const float before = d.me().health;
      d.put(pool, kAt[2]);
      d.frame(0);
      r.drowned += before - d.me().health;
    }
    d.take();
    for (int i = 0; i < 5; i++) {
      d.put(kFloor, 24);
      d.frame(0);
    }
    r.out += " / after drowning: " + d.take();
  }
  return r;
}

// A list of sounds with the burn's two cries, picked at random, as one.
std::string burns(std::string heard) {
  for (size_t at = heard.find("lburn"); at != std::string::npos; at = heard.find("lburn", at + 1))
    heard[at + 5] = 'N';
  return heard;
}

void run(gl::Lab& lab) {
  Hall hall;
  {
    mc::Source src;
    std::string err;
    if (!mc::read_plan(kPlan, &src, &err)) {
      lab.fail("se-dive: the hall's plan does not read:\n" + err,
               "next: fix kPlan in games/shooters_edge/labs/se_dive_lab.cpp");
      return;
    }
    mc::CompileOptions opt;
    opt.light = false;
    mc::CompileReport rep;
    if (!mc::compile_map(src, opt, &hall.bsp, &rep, &err)) {
      lab.fail("se-dive: the hall does not compile: " + err,
               "next: run tests/brush_compile_test, then fix kPlan");
      return;
    }
  }
  sv::CampaignOptions co;
  co.server.seed = 1;
  co.server.loop.max_edicts = 1024;
  co.load = &hall_map;
  co.load_ctx = &hall;
  co.settings = {"deathmatch 0", "coop 0", "skill 1"};

  std::vector<std::pair<std::string, const pg::Progs*>> programs;
  pg::Progs own;
  std::string error;
  if (!sv::campaign_mod(own, error)) {
    lab.fail("se-dive: the campaign: " + error, "next: run tests/shooters_edge_campaign_mod_test");
    return;
  }
  programs.push_back({"ours", &own});
  pg::Progs pack;
  std::vector<uint8_t> content;
  if (!lab.arg("game").empty()) {
    ce::Source src;
    ge::vfs::Pak pak;
    if (!ce::open_source(lab.arg("game"), &src, &error) ||
        !ge::vfs::read_pak(content = src.content(), &pak, &error)) {
      lab.fail("se-dive: " + error, "next: pass --game=<a release zip, a pack or a folder>");
      return;
    }
    const ge::vfs::PakEntry* e = ge::vfs::pak_find(pak, "progs.dat");
    const std::span<const uint8_t> bytes =
        e != nullptr ? ge::vfs::pak_file(content, *e) : std::span<const uint8_t>{};
    if (e == nullptr ||
        !pg::load_progs(bytes.data(), bytes.size(), pack, error, pg::every_form())) {
      lab.fail("se-dive: the pack's program does not load: " + error,
               "next: pass the game's own folder");
      return;
    }
    programs.push_back({"pack", &pack});
  }

  gl::LabTable t{"a dive into each pool",
                 {"program", "liquid", "lost at 1, 2, 3 (2 s each)", "the harm row's",
                  "levels read", "sink u/s", "swim up u/s", "a dip (row)", "drowned in 14 s",
                  "sounds in", "sounds out", "verdict"},
                 {}};
  std::vector<Row> rows;
  int ok = 0;
  for (const auto& [name, progs] : programs) {
    for (const auto& [liquid, contents, x] : {std::tuple{"lava", sh::kContentsLava, 96.0f},
                                              std::tuple{"water", sh::kContentsWater, 320.0f},
                                              std::tuple{"slime", sh::kContentsSlime, 544.0f}}) {
      Row r = dive_into(*progs, co, liquid, contents, {x, 128, 0}, error);
      r.program = name;
      if (!error.empty()) {
        lab.fail("se-dive: the hall does not start: " + error,
                 "next: run tests/brush_compile_test");
        return;
      }
      bool good = true;
      for (int l = 0; l < 3; l++) good = good && r.lost[l] == r.want[l] && r.levels[l] == l + 1;
      good = good && r.dip == r.dip_want;
      if (name != "ours") {
        const Row& o = rows[rows.size() - 3];  // the same liquid in our campaign
        good = good && std::fabs(r.sink - o.sink) <= 1 && std::fabs(r.up - o.up) <= 1 &&
               burns(r.in) == burns(o.in) && r.out == o.out && r.drowned == o.drowned;
      }
      ok += good ? 1 : 0;
      const auto three = [](const double* v) {
        return to_fixed(v[0], 0) + ", " + to_fixed(v[1], 0) + ", " + to_fixed(v[2], 0);
      };
      t.rows.push_back(
          {r.program, r.liquid, three(r.lost), three(r.want),
           std::to_string(r.levels[0]) + std::to_string(r.levels[1]) + std::to_string(r.levels[2]),
           to_fixed(r.sink, 1), to_fixed(r.up, 1),
           to_fixed(r.dip, 0) + " (" + to_fixed(r.dip_want, 0) + ")", to_fixed(r.drowned, 0),
           r.in.empty() ? "-" : r.in, r.out.empty() ? "-" : r.out, good ? "ok" : "differs"});
      lab.metric(r.program + "." + r.liquid + ".lost", r.lost[0] + r.lost[1] + r.lost[2], "hp");
      lab.metric(r.program + "." + r.liquid + ".up", r.up, "u/s");
      rows.push_back(r);
    }
  }
  lab.table(t);
  lab.metric("dives", static_cast<double>(rows.size()), "count");
  lab.metric("dives.ok", ok, "count");
}

const gl::LabSpec kSpec{
    "se-dive",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "plugins/shooter/liquid_harm.hpp on the campaign host: lava and slime's damage and swims",
    "shooter",
    "Does a player in lava and slime take the classic damage by the level, on its clock, and swim "
    "the classic speeds, in our campaign and in the pack's own program?",
    {
        {"game", "", "the player's own release zip, pack or folder: the pack's program dives too"},
    },
    "",
    run,
};

GE_LAB(kSpec);

}  // namespace
