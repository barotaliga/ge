// FALL LINE'S ARMY VEHICLES, HELD (games/fall_line/sim/army_vehicle.hpp): none called under three
// stars or with the hunt's vehicles switched off; at three stars on seed 38 the first wave's
// carrier and truck set off, drive up and put their men down, none of them stuck; at five a tank
// stands off him and, where the army shoots to kill, runs him down, and where it does not, keeps
// its distance; every vehicle goes home once the hunt is over; and their drive on seed 38, held bit
// for bit by tests/golden/fall_line_army_vehicle.txt.
//
// A deliberate change to the vehicles moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_army_vehicle_test`, and put the
// `army-vehicles` lab's table before and after in the PR.
//
// ge:summary Test: the army's vehicles: the call, the drive, the drop, the tank, home; golden
// ge:keywords test army-vehicles carrier truck tank drop stand-off ram strike home golden
// ge:skill sim testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_vehicle.hpp"
#include "fall_line/sim/army_vehicle_rules.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "golden.hpp"
#include "skier/body.hpp"
#include "skier/skier.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace av = fall_line::sim::army_vehicle;
namespace lm = ge::core::libm;
constexpr const char* kGolden = "tests/golden/fall_line_army_vehicle.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

std::string hex(uint64_t u) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u));
  return b;
}

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(38, 10));
  return level;
}

// How a hunt is staged.
struct Stage {
  int32_t stars = 3;
  double limit = 180;   // s
  double stop = -1;     // the hunt ended at this clock, s (never if negative)
  bool lethal = true;   // whether the army shoots to kill
  bool vehicles = true; // whether the waves bring their vehicles
};

// A HUNT ON SEED 38: he stands on open snow up the mountain from the army's pad (healed each step,
// so the hunt runs its length) with only the ground waves sent and their rifles off.
struct Hunt {
  int32_t kinds[3] = {};       // called, by kind
  int32_t men = 0;             // put down by them
  int32_t stuck = 0;           // put back on their way
  int32_t strikes = 0;         // `strike` events
  double tank_nearest = 1e9;   // the nearest a tank came to him, m
  bool home = false;           // every vehicle gone once the hunt was over
  std::vector<std::string> rows;  // each 10 s: the digest and every vehicle's mode and place
};

Hunt hunt(const Stage& g) {
  fs::GameState st = fs::create_free_ride(seed38(), 0);
  st.army.fire = false;
  st.army.send_helis = false;
  st.army.send_paras = false;
  st.army.send_vehicles = g.vehicles;
  const double x = st.army.pad_x + lm::sin(st.army.pad_heading) * 400;
  const double z = st.army.pad_z + lm::cos(st.army.pad_heading) * 400;
  st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
  fs::set_wanted(st, g.stars);
  st.wanted.lethal = g.lethal;
  const fs::Controls none{};
  Hunt h;
  for (int64_t i = 0; i < std::llround(g.limit * fs::kHz); i++) {
    fs::WantedState& w = st.wanted;
    if (g.stop >= 0 && st.t >= g.stop && w.ended == fs::WantedEnd::running) {
      w.ended = fs::WantedEnd::escaped;
    }
    if (w.ended == fs::WantedEnd::running && w.stars > 0) w.heat = g.stars;
    st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
    st.racers[0].body = sk::BodyState{};
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (const fs::GameEvent& e : st.events) {
      if (e.kind != fs::EventKind::army_vehicle) continue;
      const auto c = static_cast<fs::ArmyVehicleChange>(e.gate);
      h.strikes += c == fs::ArmyVehicleChange::strike ? 1 : 0;
      if (c != fs::ArmyVehicleChange::called) continue;
      for (const fs::ArmyVehicle& v : st.army.vehicles) {
        if (v.id == static_cast<int32_t>(e.value)) h.kinds[static_cast<size_t>(v.kind)]++;
      }
    }
    for (const fs::ArmyVehicle& v : st.army.vehicles) {
      if (v.kind != fs::ArmyVehicleKind::tank || !fs::on_snow(v)) continue;
      const double d = ge::core::hypot(v.x - x, v.z - z);
      h.tank_nearest = d < h.tank_nearest ? d : h.tank_nearest;
    }
    if (g.stop >= 0 && st.t > g.stop && st.army.vehicles.empty()) h.home = true;
    if (st.steps % static_cast<int64_t>(10 * fs::kHz) == 0) {
      std::string row = hex(fs::army_vehicles_hash(st.army, 0xcbf29ce484222325ull)) + " " +
                        std::to_string(st.army.unloaded);
      for (const fs::ArmyVehicle& v : st.army.vehicles) {
        row += " " + std::string(fs::army_vehicle_kind_name(v.kind)) + ":" +
               fs::army_vehicle_mode_name(v.mode) + ":" + std::to_string(std::llround(v.x)) + "," +
               std::to_string(std::llround(v.z));
      }
      h.rows.push_back(row);
    }
  }
  h.men = st.army.unloaded;
  h.stuck = st.army.vehicles_stuck;
  return h;
}

const Hunt& three() {
  static const Hunt h = hunt({3, 180, -1, true, true});
  return h;
}

const Hunt& five() {
  static const Hunt h = hunt({5, 150, -1, true, true});
  return h;
}

TEST(no_vehicle_is_called_under_three_stars) {
  const Hunt h = hunt({2, 150, -1, true, true});
  CHECK_MSG(h.kinds[0] + h.kinds[1] + h.kinds[2] == 0, "a vehicle called at two stars");
}

TEST(no_vehicle_is_called_with_them_switched_off) {
  const Hunt h = hunt({5, 90, -1, true, false});
  CHECK_MSG(h.kinds[0] + h.kinds[1] + h.kinds[2] == 0, "a vehicle called with them off");
}

TEST(at_three_stars_a_carrier_and_a_truck_put_their_men_down) {
  const Hunt& h = three();
  CHECK_MSG(h.kinds[0] >= 1, std::to_string(h.kinds[0]) + " carriers called at three stars");
  CHECK_MSG(h.kinds[1] >= 1, std::to_string(h.kinds[1]) + " trucks called at three stars");
  CHECK_MSG(h.kinds[2] == 0, std::to_string(h.kinds[2]) + " tanks called at three stars");
  CHECK_MSG(h.men > 0, "no man put down in 180 s at three stars");
  CHECK_MSG(h.stuck == 0, "vehicles put back on their way " + std::to_string(h.stuck) + " times");
}

TEST(at_five_stars_a_tank_runs_him_down_where_the_army_shoots_to_kill) {
  const Hunt& h = five();
  CHECK_MSG(h.kinds[2] >= 1, std::to_string(h.kinds[2]) + " tanks called at five stars");
  CHECK_MSG(h.strikes > 0, "no vehicle struck him in 150 s at five stars, shooting to kill");
  CHECK_MSG(h.stuck == 0, "vehicles put back on their way " + std::to_string(h.stuck) + " times");
}

TEST(where_the_army_does_not_shoot_to_kill_the_tanks_keep_their_distance) {
  const Hunt h = hunt({5, 150, -1, false, true});
  CHECK_MSG(h.strikes == 0, std::to_string(h.strikes) + " strikes with the army not shooting");
  CHECK_MSG(h.tank_nearest > av::kTankArrestM * 0.8,
            "a tank came to " + std::to_string(h.tank_nearest) + " m of him, standing off " +
                std::to_string(av::kTankArrestM) + " m");
}

TEST(once_the_hunt_is_over_every_vehicle_goes_home) {
  const Hunt h = hunt({5, 250, 100, true, true});
  CHECK_MSG(h.home, "a vehicle was still out 150 s after the hunt ended");
  CHECK_MSG(h.stuck == 0, "vehicles put back on their way " + std::to_string(h.stuck) + " times");
}

TEST(the_vehicles_on_seed_38_keep_their_golden) {
  // Three stars for 180 s, then five for 150 s, only the ground waves sent, their rifles off. Each
  // 10 s: the vehicles' digest, the men they put down so far, every vehicle's kind, mode and place.
  std::vector<std::string> got;
  for (const auto& r : three().rows) got.push_back("3 " + r);
  for (const auto& r : five().rows) got.push_back("5 " + r);
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# each 10 s: stars, the vehicles' digest, men put down so far, every vehicle "
           "kind:mode:x,z (fall_line_army_vehicle_test, seed 38)\n";
    for (const std::string& row : got) out << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  CHECK(want.size() == got.size());
  for (size_t r = 0; r < got.size() && r < want.size(); r++) {
    std::string line;
    for (const auto& w : want[r]) line += (line.empty() ? "" : " ") + w;
    CHECK_MSG(line == got[r], "the army's vehicles on seed 38 moved at row " + std::to_string(r) +
                                  ":\n  got  " + got[r] + "\n  want " + line +
                                  "\nIf the move is meant, rewrite " + kGolden +
                                  " with GE_GOLDEN_WRITE=1 and put the army-vehicles lab's table "
                                  "before and after in the PR.");
  }
}

}  // namespace
