// LAB `se-gz-bosses`: GROUND ZERO'S BOSSES ON THE QUAKE II RUNNER, under the pack's switch
// (server/q2_gz_bosses.cpp, their rows in q2_monster_kinds.hpp), in a hall built here (no game
// files):
//
// - **Their fights**: the carrier, the black widow and her second stage each face a sturdy, still
//   player 400 units away for `--seconds` at skill 1: the damage dealt over time, the brood each
//   has out over time (fliers, stalkers), the moves each played over time, and what each fired.
// - **Their deaths**: each killed with one blow: when the widow beams out, when her legs stand and
//   when they are gone; the second stage's stalkers dying with her, when she settles and that she
//   can then be thrown apart.
//
// ge:summary Lab: Ground Zero's carrier and black widow on the runner: fights, brood, deaths
// ge:keywords lab se-gz-bosses q2 ground-zero rogue boss carrier black-widow widow2 stalker
// ge:keywords brood spawn-slots beam-out legs death-chain
// ge:skill shooter labs
// ge:test tests/shooters_edge_q2_gz_bosses_test.cpp
#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

using Pairs = std::vector<std::pair<std::string, std::string>>;
constexpr int kSturdy = 100000;  // the player's health: never killed, so every blow counts
constexpr int kSecond = 77;      // host ticks of 13 ms

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A walled hall floored at 0 and roofed at 320, the player at its west end.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& things, std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -384, -16}, hi[3] = {1024, 384, 0};
  const float roof_lo[3] = {-1024, -384, 320}, roof_hi[3] = {1024, 384, 336};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(roof_lo, roof_hi, sh::kSqSolid, "world/roof"));
  for (const float side : {-1.0f, 1.0f}) {  // its walls: a thrown player stays in
    const float end_lo[3] = {side < 0 ? -1040.0f : 1024.0f, -384, 0};
    const float end_hi[3] = {side < 0 ? -1024.0f : 1040.0f, 384, 320};
    const float wall_lo[3] = {-1024, side < 0 ? -400.0f : 384.0f, 0};
    const float wall_hi[3] = {1024, side < 0 ? -384.0f : 400.0f, 320};
    w.models[0].push_back(br::box_brush(end_lo, end_hi, sh::kSqSolid, "world/wall"));
    w.models[0].push_back(br::box_brush(wall_lo, wall_hi, sh::kSqSolid, "world/wall"));
  }
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-400 0 24"}, {"angle", "0"}}));
  for (const Pairs& t : things) w.entities.push_back(entity(t));
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

struct Run {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
};

std::unique_ptr<Run> start(const std::vector<Pairs>& things, std::string& error) {
  const auto map = hall(things, error);
  if (map == nullptr) return nullptr;
  auto run = std::make_unique<Run>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
  run->r = sv::q2_runner(sv::make_q2_server(o));
  run->r.console("deathmatch 0");
  run->r.console("skill 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  if (!run->r.start(spec, error)) return nullptr;
  run->r.connect(0, "Ana", false, false);
  return run;
}

void tick(Run& run) {
  sh::Packet pk;
  pk.client = 0;
  pk.sequence = ++run.sequence;
  sh::MoveCmd c;
  c.msec = 13;
  pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
  run.r.frame(&pk, 1);
}

std::string fixed(double v, int places) { return ge::core::num::to_fixed(v, places); }

int boss_row(const sv::Q2Server& s, std::string_view classname) {
  for (size_t i = 0; i < s.monsters.size(); i++)
    if (!s.monsters[i].spare && s.entities[size_t(s.monsters[i].entity)].classname == classname)
      return static_cast<int>(i);
  return -1;
}

struct Boss {
  const char* classname;
  const char* label;
  const char* origin;
};
constexpr Boss kBosses[] = {
    {"monster_carrier", "carrier", "0 0 160"},
    {"monster_widow", "black widow", "0 0 24"},
    {"monster_widow2", "black widow, second stage", "0 0 24"},
};

const char* shot_name(sv::SqShot k) {
  switch (k) {
    case sv::SqShot::bolt:
      return "bolts";
    case sv::SqShot::rocket:
      return "rockets";
    case sv::SqShot::grenade:
      return "grenades";
    case sv::SqShot::tracker:
      return "disruptor bolts";
    default:
      return "other";
  }
}

void fights(gl::Lab& lab, double seconds) {
  gl::LabTable table{"each boss facing a still player 400 units away, skill 1",
                     {"boss", "health", "first hurt (s)", "damage", "brood at most", "fired"},
                     {}};
  gl::LineChart dealt, brood, moves;
  dealt.spikes = brood.spikes = moves.spikes = false;
  dealt.frame = {"GROUND ZERO'S BOSSES: DAMAGE DEALT",
                 "a still player 400 units away, skill 1, under the pack's switch", "t (s)",
                 "damage dealt"};
  brood.frame = {"GROUND ZERO'S BOSSES: THEIR BROOD OUT",
                 "fliers (the carrier's) and stalkers (the widows'), within their slots", "t (s)",
                 "out"};
  moves.frame = {"GROUND ZERO'S BOSSES: THE MOVE PLAYED",
                 "0 the run; carrier 1-11, widow 12-25, second stage 26-32 (q2_gz_bosses.cpp)",
                 "t (s)", "move"};
  const int ticks = static_cast<int>(seconds * kSecond);
  for (const Boss& b : kBosses) {
    std::string error;
    std::unique_ptr<Run> run =
        start({{{"classname", b.classname}, {"origin", b.origin}, {"angle", "180"}}}, error);
    if (run == nullptr) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
    tick(*run);
    sv::Q2Server& s = run->s();
    const int row = boss_row(s, b.classname);
    if (row < 0) {
      lab.fail(std::string(b.classname) + " did not spawn under the pack",
               "Read kind_named in server/q2_monster_kinds.hpp. Skill: shooter.");
      return;
    }
    const int health = s.entities[size_t(s.monsters[size_t(row)].entity)].health;
    s.players[0].health = kSturdy;
    s.monsters[size_t(row)].enemy = 0;
    gl::Trace hurt{b.label, {}, {}}, out{b.label, {}, {}}, move{b.label, {}, {}};
    double first = -1;
    int most = 0;
    int fired[64] = {};
    std::vector<double> seen(std::size(s.missiles), -1);
    for (int i = 0; i < ticks; i++) {
      tick(*run);
      const sv::Q2Monster& m = s.monsters[size_t(row)];
      const double sofar = kSturdy - s.players[0].health;
      if (first < 0 && sofar > 0) first = (i + 1) * 0.013;
      for (size_t k = 0; k < std::size(s.missiles); k++) {
        const sv::Q2Missile& q = s.missiles[k];
        if (q.from == m.entity && q.dies_at != seen[k] && q.dies_at > 0)
          fired[static_cast<int>(q.kind) & 63]++;
        seen[k] = q.dies_at;
      }
      int n = 0;
      for (const sv::Q2Monster& o : s.monsters) n += o.commander == row && !o.spare && !o.dead;
      most = std::max(most, n);
      if (i % 8 == 0) {
        const double t = (i + 1) * 0.013;
        hurt.x.push_back(t);
        hurt.y.push_back(sofar);
        out.x.push_back(t);
        out.y.push_back(n);
        move.x.push_back(t);
        move.y.push_back(m.boss.move);
      }
    }
    const double total = kSturdy - s.players[0].health;
    std::string what;
    for (int k = 0; k < 64; k++)
      if (fired[k] > 0)
        what += (what.empty() ? "" : ", ") + std::to_string(fired[k]) + " " +
                shot_name(static_cast<sv::SqShot>(k));
    if (what.empty()) what = "lines (bullets, rails, beams)";
    const std::string key = std::string("boss.") + b.classname;
    lab.metric(key + ".damage", total, "points");
    lab.metric(key + ".first-hurt", first, "s");
    lab.metric(key + ".brood", most, "monsters");
    table.rows.push_back({b.label, std::to_string(health), first < 0 ? "none" : fixed(first, 2),
                          fixed(total, 0), std::to_string(most), what});
    dealt.traces.push_back(std::move(hurt));
    brood.traces.push_back(std::move(out));
    moves.traces.push_back(std::move(move));
  }
  lab.table(table);
  const std::vector<std::pair<std::string, std::string>> args{{"seconds", fixed(seconds, 0)}};
  lab.shot({"damage", dealt.frame.title,
            "The carrier, the black widow and her second stage, each 400 units from a still "
            "player at skill 1, under Ground Zero's switch: the damage dealt so far.",
            "steps for the rails (50), the rockets and grenades; a steady climb under the "
            "machine guns, the widow's blaster and the heat beam",
            args},
           gl::to_picture(gl::line_chart(dealt).plot));
  lab.shot({"brood", brood.frame.title,
            "How many of each boss's brood are out: the carrier's fliers (6 slots at skill 1), "
            "the widows' stalkers (3).",
            "the carrier's fliers three a launch, the widows' stalkers two at a time; never "
            "above their slots",
            args},
           gl::to_picture(gl::line_chart(brood).plot));
  lab.shot({"moves", moves.frame.title,
            "The move each boss plays by hand, frame by frame (0: the base game's run carries it).",
            "the carrier between its guns (1-3), grenades (4-6), rockets (7), rail (8) and "
            "launch (9); the widow's blaster (12-16), rails (17-20), charge (21), spawn (22) and "
            "kick (23); the second stage's beam (26-28), disruptor (29), spawn (30), tongue (31)",
            args},
           gl::to_picture(gl::line_chart(moves).plot));
}

void deaths(gl::Lab& lab) {
  gl::LabTable table{"each boss killed with one blow at skill 1", {"boss", "event", "at (s)"}, {}};
  for (const Boss& b : kBosses) {
    std::string error;
    std::vector<Pairs> things{{{"classname", b.classname}, {"origin", b.origin}, {"angle", "180"}}};
    if (std::string_view(b.classname) == "monster_widow2")
      things.push_back(
          {{"classname", "monster_stalker"}, {"origin", "-200 250 24"}, {"angle", "0"}});
    std::unique_ptr<Run> run = start(things, error);
    if (run == nullptr) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
    tick(*run);
    sv::Q2Server& s = run->s();
    const int row = boss_row(s, b.classname);
    const sv::Q2Monster& m = s.monsters[size_t(row)];
    const sv::Q2Entity& e = s.entities[size_t(m.entity)];
    sv::monster_damage(s, m.entity, -1, e.health + 1, {});
    int stalkers = 0;
    for (const sv::Q2Monster& o : s.monsters)
      stalkers += o.dead && s.entities[size_t(o.entity)].classname == "monster_stalker";
    if (stalkers > 0) table.rows.push_back({b.label, "stalkers dead with her", "0"});
    double legs = -1, gone = -1, settled = -1;
    for (int i = 0; i < 20 * kSecond && gone < 0; i++) {
      s.players[0].health = kSturdy;
      tick(*run);
      const double t = (i + 1) * 0.013;
      if (legs < 0 && m.boss.legs >= 0) legs = t;
      if (settled < 0 && std::string_view(b.classname) == "monster_widow2" && e.maxs.z == 80)
        settled = t;
      if (e.gone) gone = t;
      if (settled >= 0) break;
    }
    const std::string key = std::string("death.") + b.classname;
    if (legs >= 0) {
      table.rows.push_back({b.label, "beamed out: her legs stand", fixed(legs, 2)});
      lab.metric(key + ".legs", legs, "s");
    }
    if (settled >= 0) {
      table.rows.push_back({b.label, "settled: she can be thrown apart", fixed(settled, 2)});
      lab.metric(key + ".settled", settled, "s");
      sv::monster_damage(s, m.entity, -1, 1000, {});
      table.rows.push_back({b.label, e.gone ? "thrown apart by a blow of 1000" : "not thrown apart",
                            fixed(settled, 2)});
    }
    if (gone >= 0) {
      table.rows.push_back({b.label, "gone", fixed(gone, 2)});
      lab.metric(key + ".gone", gone, "s");
    }
  }
  lab.table(table);
}

void run(gl::Lab& lab) {
  fights(lab, lab.arg_num("seconds"));
  deaths(lab);
}

const gl::LabSpec kSpec{
    "se-gz-bosses",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "Ground Zero's carrier and black widow (both stages) on Quake II's runner under the pack's "
    "switch (server/q2_gz_bosses.cpp)",
    "shooter",
    "Do Ground Zero's bosses fight, bring their brood within their slots and die as the pack's "
    "notes say?",
    {{"seconds", "30", "seconds each boss faces the player"}},
    "--seconds=6",
    run,
};
GE_LAB(kSpec);

}  // namespace
