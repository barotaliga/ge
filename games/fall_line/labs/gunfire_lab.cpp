// LAB `gunfire`: the army's guns and what their rounds do to him. Two tables:
// - THE RANGE: on the flat drag strip, each gun (the rifle, the snowmobile's light machine gun,
//   the door gun) fires `--bursts` bursts at him from each range, under aimed fire and warning
//   fire, at a man stood facing it and one crossing it at `--cross` m/s. Per row: the rounds fired,
//   the rounds into him and their share, the energy a hit carried, J, the part struck most, and
//   the rounds that cracked past him. He is healed and stood again after every step, so every
//   round meets the same man; the shooter's hold is the weapon's own times the hunt's widening at
//   `--stars`.
// - THE HUNT: on a real ski area, he stands on open snow up the mountain from the village, the
//   army's every arm sent at `--hunt-stars` and firing through the real `step`, where the army
//   shoots to kill (`open`) and where it does not (`spared`). Per scenario: the rounds the
//   soldiers, the snowmobiles' gunners and the helicopters' door gunners fired, the rounds into
//   him, his injury severity score, and when the hunt ended and how.
// - THE DOORS: the same hunt at one, two and three stars, the rules of fire `hold`, `warn` and
//   `aim`: the door gunners' rounds, every arm's rounds, the rounds into him, and the hunt's end;
//   and a picture each (`doors-<rules>.png`), the machines' paths round him and every door round
//   from its muzzle, coloured by the door it left.
//
// A gun that never hits a still man at 100 m under aimed fire, warning fire that ever strikes him,
// a hunt that shoots to kill and fires nothing, a door gunner firing under `hold` or never under
// `aim` is a failure (exit 1).
//
// ge:summary Lab: the army's guns: hits by weapon, range and rules, and a seed-38 hunt's fire
// ge:keywords lab gunfire rifle lmg door-gun range hit-share warning-fire aimed-fire wound iss
// ge:keywords killed hunt snap door-gunner doors
// ge:skill sim labs
// ge:test tests/fall_line_gunfire_test.cpp
#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_heli_rules.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/gunfire.hpp"
#include "fall_line/sim/wanted.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "skier/anatomy.hpp"
#include "skier/body.hpp"
#include "skier/skier.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace lm = ge::core::libm;
namespace ah = fall_line::sim::army_heli;
using ge::lab::Rgba;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

constexpr double kMid = sk::kStripSize / 2;
constexpr std::array<fs::WeaponKind, 3> kWeapons{fs::WeaponKind::rifle, fs::WeaponKind::lmg,
                                                 fs::WeaponKind::door_gun};
constexpr std::array<double, 6> kRanges{50, 100, 200, 300, 400, 600};

struct Range {
  int32_t fired = 0;
  int32_t hits = 0;
  int32_t snaps = 0;
  double energy = 0;
  std::array<int32_t, sk::kParts> by_part{};
};

// One row of the range: `bursts` bursts from `range` m at him, stood (`cross` 0) or crossing.
Range range_fire(const std::shared_ptr<const mg::Level>& strip, fs::WeaponKind weapon, double range,
                 fs::FireRule rules, double cross, int32_t bursts, int32_t stars) {
  fs::GameState st = fs::create_game_on(strip, 1);
  if (stars > 0) {
    st.army.fire = false;
    st.army.send_helis = false;
    fs::set_wanted(st, stars);
  }
  // Crossing from his left to his right across the gun's line, through its foot at the middle
  // of the bursts.
  const double from = -cross * 4;
  const auto stand = [&st, cross, from](double t) {
    const double x = kMid + from + cross * t;
    st.racers[0].skier =
        sk::stand_skier(st.world(), x, kMid, cross > 0 ? 1.5707963267948966 : 0, cross);
  };
  stand(0);
  const double gy = st.level->ground_at(kMid, kMid + range) + 1.45;
  fs::Gun* gun = fs::arm_gun(st, 1, weapon, kMid, gy, kMid + range);
  int32_t shot = 0;
  for (int64_t i = 0; i < std::llround(240 * fs::kHz) &&
                      (shot < bursts || gun->left > 0 || !st.gunfire.rounds.empty());
       i++) {
    if (stars > 0) st.wanted.heat = stars;
    if (shot < bursts && fs::gun_ready(*gun) && fs::fire_at(st, *gun, fs::AimTarget{}, rules)) {
      shot++;
    }
    st.events.clear();
    fs::step_gunfire(st);
    st.steps++;
    st.t = static_cast<double>(st.steps) * fs::kDt;
    st.racers[0].body = sk::BodyState{};
    // A crossing man is brought back to his start each burst, so every burst meets him crossing.
    stand(cross > 0 ? ge::core::num::mod(st.t, 8) : 0);
  }
  Range r;
  r.fired = st.gunfire.fired;
  r.hits = st.gunfire.hits;
  r.snaps = st.gunfire.snaps;
  r.energy = st.gunfire.energy;
  r.by_part = st.gunfire.by_part;
  return r;
}

// What THE DOORS' pictures draw: each machine's path, and each door round from the muzzle it left
// to where it was last seen, with the door it came from.
struct DoorRound {
  fs::Spot from, to;
  bool inward = false;
  bool warn = false;
  bool seen = false;  // a door round (the list is by round id, every gun's)
};
struct Trace {
  fs::Spot q;
  std::vector<std::vector<fs::Spot>> paths;  // by machine id
  std::vector<DoorRound> rounds;
};

struct Hunt {
  int32_t rifle = 0;
  int32_t lmg = 0;
  int32_t door = 0;
  int32_t hits = 0;
  int32_t iss = 0;
  double ended = -1;
  fs::WantedEnd how = fs::WantedEnd::running;
};

void trace_step(const fs::GameState& st, Trace& tr) {
  if (st.steps % static_cast<int64_t>(fs::kHz / 4) == 0) {
    for (const fs::ArmyHeli& m : st.army.helis) {
      if (tr.paths.size() <= static_cast<size_t>(m.id)) tr.paths.resize(m.id + 1);
      tr.paths[static_cast<size_t>(m.id)].push_back(fs::Spot{m.x, 0, m.z});
    }
  }
  for (const fs::Round& r : st.gunfire.rounds) {
    if (r.weapon != fs::WeaponKind::door_gun) continue;
    const int32_t k = r.gun - ah::kGunBase;
    // A round met for the first time: where it left from, and which door that was.
    if (tr.rounds.size() <= static_cast<size_t>(r.id)) tr.rounds.resize(r.id + 1);
    DoorRound& d = tr.rounds[static_cast<size_t>(r.id)];
    if (r.age <= fs::kDt * 1.5) {
      d.from = fs::Spot{r.x, 0, r.z};
      d.seen = true;
      d.warn = r.warn;
      for (const fs::ArmyHeli& m : st.army.helis) {
        if (m.id == k / 2) d.inward = (k % 2 == 0) == (m.dir > 0);
      }
    }
    d.to = fs::Spot{r.x, 0, r.z};
  }
}

// THE DOORS' PICTURE: `half` m either side of him, from above (+x right, +z down).
constexpr double kHalfM = 700;
constexpr Rgba kPathColours[] = {{0, 140, 220}, {120, 60, 200}, {0, 150, 90}, {150, 110, 0}};

ge::lab::Picture draw_doors(const Trace& tr, int px) {
  ge::lab::Picture pic = ge::lab::create_picture(px, px, Rgba{236, 240, 246});
  const double k = px / (2 * kHalfM);
  const auto at = [&tr, k](const fs::Spot& s, double& u, double& v) {
    u = (s.x - tr.q.x + kHalfM) * k;
    v = (s.z - tr.q.z + kHalfM) * k;
  };
  for (const double r : {100.0, ah::kOrbitM, 400.0, 600.0}) {
    ge::lab::draw_ring(pic, kHalfM * k, kHalfM * k, r * k, Rgba{200, 206, 216});
  }
  ge::lab::draw_ring(pic, kHalfM * k, kHalfM * k, ah::kOrbitM * k, Rgba{150, 160, 176});
  for (size_t i = 0; i < tr.paths.size(); i++) {
    const Rgba c = kPathColours[i % std::size(kPathColours)];
    for (size_t j = 1; j < tr.paths[i].size(); j++) {
      double u0, v0, u1, v1;
      at(tr.paths[i][j - 1], u0, v0);
      at(tr.paths[i][j], u1, v1);
      ge::lab::draw_line(pic, u0, v0, u1, v1, c, 1);
    }
  }
  for (const DoorRound& d : tr.rounds) {
    if (!d.seen) continue;
    double u0, v0, u1, v1;
    at(d.from, u0, v0);
    at(d.to, u1, v1);
    ge::lab::draw_line(pic, u0, v0, u1, v1, d.warn ? Rgba{230, 180, 40} : Rgba{220, 50, 40}, 1);
  }
  for (const DoorRound& d : tr.rounds) {
    if (!d.seen) continue;
    double u, v;
    at(d.from, u, v);
    ge::lab::fill_disc(pic, u, v, 2.5, d.inward ? Rgba{0, 170, 60} : Rgba{255, 120, 0});
  }
  ge::lab::fill_disc(pic, kHalfM * k, kHalfM * k, 5, Rgba{255, 0, 255});
  return pic;
}

Hunt hunt(const std::shared_ptr<const mg::Level>& level, int32_t stars, bool lethal, double limit,
          Trace* trace = nullptr) {
  fs::GameState st = fs::create_free_ride(level, 0);
  st.wanted.lethal = lethal;
  const double fx = lm::sin(st.army.pad_heading);
  const double fz = lm::cos(st.army.pad_heading);
  const double x = st.army.pad_x + fx * 400;
  const double z = st.army.pad_z + fz * 400;
  st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
  fs::set_wanted(st, stars);
  const fs::Controls none{};
  Hunt h;
  if (trace != nullptr) trace->q = fs::Spot{x, 0, z};
  for (int64_t i = 0; i < std::llround(limit * fs::kHz); i++) {
    fs::WantedState& w = st.wanted;
    if (w.ended != fs::WantedEnd::running) {
      h.ended = w.stopped;
      h.how = w.ended;
      break;
    }
    if (w.stars > 0) w.heat = stars;
    if (!st.racers[0].skier.down) st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (const fs::GameEvent& e : st.events) {
      if (e.kind != fs::EventKind::gunfire || fs::gunfire_phase(e) != fs::GunfirePhase::shot) {
        continue;
      }
      h.rifle += e.value == static_cast<double>(fs::WeaponKind::rifle) ? 1 : 0;
      h.lmg += e.value == static_cast<double>(fs::WeaponKind::lmg) ? 1 : 0;
      h.door += e.value == static_cast<double>(fs::WeaponKind::door_gun) ? 1 : 0;
    }
    if (trace != nullptr) trace_step(st, *trace);
  }
  h.hits = st.gunfire.hits;
  h.iss = sk::severity_of(st.racers[0].body);
  return h;
}

void run(ge::lab::Lab& lab) {
  const auto bursts = static_cast<int32_t>(lab.arg_num("bursts"));
  const double cross = lab.arg_num("cross");
  const auto stars = static_cast<int32_t>(lab.arg_num("stars"));
  const auto hunt_stars = static_cast<int32_t>(lab.arg_num("hunt-stars"));
  const double limit = lab.arg_num("limit");
  const double seed = lab.arg_num("seed");
  const auto version = static_cast<int>(lab.arg_num("version"));
  const int px = static_cast<int>(lab.arg_num("px"));
  const bool pictures = !lab.arg_bool("no-pictures");
  const auto strip = std::make_shared<const mg::Level>(sk::flat_strip(0, 1));

  ge::lab::LabTable range{"range",
                          {"weapon", "range m", "rules", "target", "fired", "hits", "share",
                           "J a hit", "most struck", "snaps"},
                          {}};
  for (const fs::WeaponKind weapon : kWeapons) {
    const std::string wn = fs::weapon_name(weapon);
    for (const double d : kRanges) {
      if (d > fs::weapons::gun_def(weapon).range + 200) continue;
      for (const fs::FireRule rules : {fs::FireRule::aim, fs::FireRule::warn}) {
        for (const double c : {0.0, cross}) {
          const Range r = range_fire(strip, weapon, d, rules, c, bursts, stars);
          int32_t most = -1;
          for (int32_t p = 0; p < sk::kParts; p++) {
            if (r.by_part[static_cast<size_t>(p)] > 0 &&
                (most < 0 ||
                 r.by_part[static_cast<size_t>(p)] > r.by_part[static_cast<size_t>(most)])) {
              most = p;
            }
          }
          const std::string rn = rules == fs::FireRule::aim ? "aim" : "warn";
          const std::string tn = c > 0 ? "crossing" : "still";
          const double share = r.fired > 0 ? static_cast<double>(r.hits) / r.fired : 0;
          range.rows.push_back({wn, fixed(d, 0), rn, tn, std::to_string(r.fired),
                                std::to_string(r.hits), fixed(share, 2),
                                r.hits > 0 ? fixed(r.energy / r.hits, 0) : std::string("-"),
                                most >= 0 ? sk::part_name(static_cast<sk::Part>(most)) : "-",
                                std::to_string(r.snaps)});
          std::string id = wn;
          id += "." + fixed(d, 0);
          id += ".";
          id += rn;
          id += ".";
          id += tn;
          lab.metric(id + ".share", share, "share");
          if (rules == fs::FireRule::warn && r.hits > 0) {
            lab.fail(id + ": warning fire struck him " + std::to_string(r.hits) + " times",
                     "Read fire_at's warning spot and fly_round's body test in "
                     "games/fall_line/sim/gunfire.cpp (skill: sim).");
          }
          if (rules == fs::FireRule::aim && d == 100 && c == 0 && r.hits == 0) {
            lab.fail(id + ": aimed fire at a still man at 100 m never struck him",
                     "Read fire_round's aim and body_hit in games/fall_line/sim/gunfire.cpp and "
                     "gunfire_body.cpp (skill: sim).");
          }
        }
      }
    }
  }
  lab.table(std::move(range));

  std::shared_ptr<const mg::Level> level;
  try {
    level = std::make_shared<const mg::Level>(fs::free_map(seed, version));
  } catch (const mg::GenerateError& e) {
    lab.fail("seed " + fixed(seed, 0) + " built no map: " + e.what(),
             "Run `make lab LAB=analyze ARGS=--seeds=" + fixed(seed, 0) +
                 " --refusals` (skill: mountain).");
    return;
  }
  const auto& resort = level->resort;
  if (!resort || resort->lifts.empty()) {
    lab.fail("seed " + fixed(seed, 0) + " v" + fixed(version, 0) + " has no ski area",
             "Pass a --version that builds a ski area (10 or later; skill: mountain).");
    return;
  }
  ge::lab::LabTable table{"hunt",
                          {"scenario", "stars", "rifle rounds", "lmg rounds", "door rounds", "hits",
                           "iss", "ended s", "how"},
                          {}};
  for (const bool lethal : {true, false}) {
    const std::string id = lethal ? "open" : "spared";
    const Hunt h = hunt(level, hunt_stars, lethal, limit);
    table.rows.push_back(
        {id, std::to_string(hunt_stars), std::to_string(h.rifle), std::to_string(h.lmg),
         std::to_string(h.door), std::to_string(h.hits), std::to_string(h.iss),
         h.ended >= 0 ? fixed(h.ended, 2) : std::string("-"), fs::wanted_end_name(h.how)});
    lab.metric(id + ".rifle", h.rifle, "rounds");
    lab.metric(id + ".lmg", h.lmg, "rounds");
    lab.metric(id + ".door", h.door, "rounds");
    lab.metric(id + ".hits", h.hits, "rounds");
    lab.metric(id + ".iss", h.iss, "iss");
    lab.metric(id + ".ended", h.ended, "s");
    if (lethal && h.rifle + h.lmg + h.door == 0) {
      lab.fail("open: the army shot to kill and fired nothing in " + fixed(limit, 0) + " s",
               "Read hold in games/fall_line/sim/army.cpp and halt in army_sled.cpp (skill: sim).");
    }
    if (!lethal && h.hits > 0) {
      lab.fail("spared: a round struck him where the army does not shoot to kill",
               "Read rules_of and fly_round in games/fall_line/sim/gunfire.cpp (skill: sim).");
    }
  }
  lab.table(std::move(table));

  // THE DOOR GUNNERS at each rule of fire: one star (`hold`), two (`warn`), three (`aim`).
  ge::lab::LabTable doors{
      "doors",
      {"stars", "rules", "door rounds", "all rounds", "hits", "iss", "ended s", "how"},
      {}};
  for (int32_t s = 1; s <= 3; s++) {
    Trace tr;
    const Hunt h = hunt(level, s, true, limit, pictures ? &tr : nullptr);
    const fs::FireRule rules = fs::roster_of(s).fire;
    const std::string rn = rules == fs::FireRule::aim    ? "aim"
                           : rules == fs::FireRule::warn ? "warn"
                                                         : "hold";
    doors.rows.push_back(
        {std::to_string(s), rn, std::to_string(h.door), std::to_string(h.rifle + h.lmg + h.door),
         std::to_string(h.hits), std::to_string(h.iss),
         h.ended >= 0 ? fixed(h.ended, 2) : std::string("-"), fs::wanted_end_name(h.how)});
    lab.metric("doors." + rn + ".rounds", h.door, "rounds");
    lab.metric("doors." + rn + ".hits", h.hits, "rounds");
    if (rules == fs::FireRule::hold && h.door > 0) {
      lab.fail("doors.hold: the door gunners fired " + std::to_string(h.door) +
                   " rounds under the hold rules",
               "Read doors_manned in games/fall_line/sim/army_door.cpp (skill: sim).");
    }
    if (rules == fs::FireRule::warn && h.hits > 0) {
      lab.fail("doors.warn: a round struck him under warning fire",
               "Read rules_of and fly_round in games/fall_line/sim/gunfire.cpp (skill: sim).");
    }
    if (rules == fs::FireRule::aim && h.door == 0) {
      lab.fail("doors.aim: the door gunners never fired in " + fixed(limit, 0) + " s",
               "Read step_army_doors's arc and sight in games/fall_line/sim/army_door.cpp "
               "(skill: sim).");
    }
    if (!pictures) continue;
    int32_t inward = 0;
    for (const DoorRound& d : tr.rounds) inward += d.seen && d.inward ? 1 : 0;
    lab.shot(
        {"doors-" + rn,
         "SEED " + fixed(seed, 0) + " DOOR GUNNERS: " + std::to_string(s) + " STARS (" + rn + ")",
         "1.4 km square round him (magenta dot) from above, +x right, +z down; grey rings at "
         "100, 220 (the pylon turn), 400 and 600 m. Each army helicopter's path, a colour a "
         "machine. Every door-gun round as a line from its muzzle to where it was last "
         "seen (red aimed, amber warning) and a dot at the muzzle: green from the door "
         "the machine turns toward, orange from the other. " +
             std::to_string(h.door) + " door rounds, " + std::to_string(inward) +
             " from the inward door, " + std::to_string(h.hits) + " into him.",
         rules == fs::FireRule::hold ? "paths and no rounds at all: the doors stay shut under hold"
         : rules == fs::FireRule::warn
             ? "amber lines that stop short of or wide of the magenta dot, never on it"
             : "red lines from the 220 m ring onto the magenta dot, green dots on the ring",
         {{"seed", fixed(seed, 0)},
          {"version", fixed(version, 0)},
          {"stars", std::to_string(s)},
          {"rules", rn},
          {"limit s", fixed(limit, 0)}}},
        draw_doors(tr, px));
  }
  lab.table(std::move(doors));
}

const ge::lab::LabSpec kSpec{
    "gunfire",
    ge::lab::LabKind::scenario,
    "game:fall_line",
    "games/fall_line/sim/gunfire (the army's guns: the aim, the bursts, the rounds' flight, where "
    "they strike his body and the wounds they do)",
    "sim",
    "How often does each of the army's guns strike a man by range and rules of fire, what does it "
    "do to him, and does a real hunt's fire wound and kill where the army shoots to kill and "
    "never touch him where it does not?",
    {
        {"bursts", "40", "bursts a range row fires"},
        {"cross", "10", "the crossing man's speed, m/s"},
        {"stars", "0", "the hunt's stars the range's shooters hold at (0: the weapon's own hold)"},
        {"hunt-stars", "4", "the stars the hunt on the ski area is pinned at"},
        {"limit", "150", "the hunt's time limit, s"},
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator's version (a ski area: 10 or later)"},
        {"px", "640", "the doors' pictures' size, px"},
        {"no-pictures", "", "skip the pictures", true},
    },
    "--bursts=8 --limit=90 --px=256",
    run,
};
GE_LAB(kSpec);

}  // namespace
