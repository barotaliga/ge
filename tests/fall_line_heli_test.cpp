// FALL LINE'S HELICOPTER ON THE FREE RIDE, HELD (games/fall_line/sim/heli_ride.hpp and
// heli_outing.hpp): parked on its pad with its rotor still and not stepped; taken on the machine
// press only from near its seat and slow enough; flown on his own controls with him held on the
// seat; dropped off in the air and flown home to its pad by its pilot; stepped off when landed,
// the machine shut down where it stands; crashed, the skier thrown and the ride started again
// from the pad; the tape carrying the lever and the pedal; and the bot's outing on seed 38 (at
// the generator's v10 rules, pinned) from its pad to the head of the highest run and down it on
// his skis, held by tests/golden/fall_line_heli.txt.
//
// A deliberate change to the machine, its place on the free ride, the bot or the generator moves
// the golden: rewrite it with `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_heli_test`, and
// put the `free` lab's heli table before and after in the PR.
//
// ge:summary Test: the free ride's helicopter: pad, board, fly, drop, home, crash, seed 38 outing
// ge:keywords test heli helicopter free-ride board drop home crash restart machine-press lever
// ge:keywords pedal tape heli-outing golden
// ge:skill sim helicopter testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/controls.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/heli_outing.hpp"
#include "fall_line/sim/heli_ride.hpp"
#include "fall_line/sim/simulate.hpp"
#include "golden.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
constexpr const char* kGolden = "tests/golden/fall_line_heli.txt";
constexpr double kSeed = 38;
constexpr int kVersion = 10;

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

// A free ride on the flat strip: the pad beside its foot, open snow all round.
fs::GameState strip_ride() {
  return fs::create_free_ride(std::make_shared<const mg::Level>(ge::skier::flat_strip(0, 1)));
}

double over_snow(const fs::GameState& state) {
  const auto& b = state.heli.body;
  return b.y - state.level->ground_at(b.x, b.z);
}

// Steps with `c`, every event kept.
std::vector<fs::GameEvent> run_for(fs::GameState& state, fs::Controls c, double seconds) {
  std::vector<fs::GameEvent> all;
  for (int i = 0; i < static_cast<int>(seconds * fs::kHz); i++) {
    fs::step(state, std::span<const fs::Controls>(&c, 1));
    all.insert(all.end(), state.events.begin(), state.events.end());
  }
  return all;
}

std::vector<fs::GameEvent> press(fs::GameState& state, fs::Controls c = {}) {
  c.machine = true;
  return run_for(state, c, 1 / fs::kHz);
}

bool said(const std::vector<fs::GameEvent>& events, fs::HeliPhase phase) {
  for (const fs::GameEvent& e : events) {
    if (e.kind == fs::EventKind::heli && e.gate == static_cast<int32_t>(phase)) return true;
  }
  return false;
}

// Sat on the skid, the rotor spooled up (some 4.5 s) and lifted on the lever for `seconds`: some
// 2 m over the strip at 6 s, some 20 m at 14.
fs::GameState aloft(double seconds) {
  fs::GameState state = strip_ride();
  fs::stand_by_heli(state, 0);
  press(state);
  fs::Controls up;
  up.lever = 0.62;
  run_for(state, up, seconds);
  return state;
}

}  // namespace

TEST(the_machine_waits_on_its_pad_with_its_rotor_still_and_is_not_stepped) {
  fs::GameState state = strip_ride();
  const fs::FreeHeli& m = state.heli;
  CHECK(m.present && !m.rider && m.mode == fs::HeliMode::parked && m.taken == 0);
  CHECK(m.body.grounded && m.body.spool == 0);
  CHECK(std::abs(m.body.x - m.pad_x) < 1e-9 && std::abs(m.body.z - m.pad_z) < 1e-9);
  const double x = m.body.x, y = m.body.y, z = m.body.z;
  run_for(state, fs::Controls{}, 3);
  CHECK(m.body.x == x && m.body.y == y && m.body.z == z && m.body.rotor == 0);
}

TEST(the_press_sits_him_on_the_skid_only_from_beside_it_and_slow_enough) {
  fs::GameState state = strip_ride();
  CHECK(!fs::heli_within(state, 0));
  CHECK(!said(press(state), fs::HeliPhase::board) && !fs::on_heli(state, 0));
  fs::stand_by_heli(state, 0);
  CHECK(fs::heli_within(state, 0));
  state.racers[0].skier.speed = fs::heli_ride::kBoardFastest + 2;
  CHECK(!fs::heli_within(state, 0));
  fs::stand_by_heli(state, 0);
  CHECK(said(press(state), fs::HeliPhase::board));
  CHECK(fs::on_heli(state, 0) && state.heli.taken == 1 && state.heli.mode == fs::HeliMode::flown);
  // The press that sat him there is spent: he is still on it.
  run_for(state, fs::Controls{}, 1);
  CHECK(fs::on_heli(state, 0));
}

TEST(flown_on_his_lever_it_lifts_him_held_on_the_seat) {
  fs::GameState state = aloft(14);
  const auto& k = state.racers[0].skier;
  CHECK_MSG(over_snow(state) > 10, "fourteen seconds on the lever lifted it " +
                                       std::to_string(over_snow(state)) +
                                       " m; run `make lab LAB=heli` (skill: helicopter)");
  const ge::core::Vec3 seat = fs::heli_seat(state.heli.body);
  CHECK(std::abs(k.x - seat.x) < 1e-9 && std::abs(k.y - seat.y) < 1e-9 && !k.down);
  CHECK(!k.on_snow);
}

TEST(dropped_in_the_air_he_falls_onto_his_skis_and_its_pilot_flies_it_home) {
  fs::GameState state = aloft(6);
  CHECK(!state.heli.body.grounded);
  fs::Controls hold;
  hold.lever = state.heli.flown.collective;
  CHECK(said(press(state, hold), fs::HeliPhase::drop));
  CHECK(!fs::on_heli(state, 0) && state.heli.mode == fs::HeliMode::home);
  CHECK(!state.racers[0].skier.on_snow);
  // Pushed off over the open strip: home it goes and shuts down on its pad.
  const std::vector<fs::GameEvent> all = run_for(state, fs::Controls{}, 90);
  CHECK_MSG(said(all, fs::HeliPhase::home) && state.heli.mode == fs::HeliMode::parked,
            "the pilot did not fly it home; read games/fall_line/sim/heli_ride.cpp (skill: sim)");
  CHECK(std::hypot(state.heli.body.x - state.heli.pad_x, state.heli.body.z - state.heli.pad_z) <
        fs::heli_ride::kHomeNear);
  CHECK(state.racers[0].progress.wipeouts == 0);
}

TEST(landed_he_steps_off_and_it_shuts_down_where_it_stands) {
  fs::GameState state = strip_ride();
  fs::stand_by_heli(state, 0);
  press(state);
  fs::Controls up;
  up.lever = 0.55;
  run_for(state, up, 7);
  CHECK(!state.heli.body.grounded);
  // The lever eased down: it settles back onto its skids.
  fs::Controls down;
  down.lever = 0.3;
  run_for(state, down, 10);
  CHECK_MSG(state.heli.body.grounded && state.heli.mode == fs::HeliMode::flown,
            "eased down it did not sit on its skids (crash: " +
                std::string(ge::helicopter::heli_crash_name(state.heli.body.crashed)) + ")");
  CHECK(said(press(state), fs::HeliPhase::drop));
  CHECK(!fs::on_heli(state, 0) && state.heli.mode == fs::HeliMode::parked);
  CHECK(state.racers[0].skier.on_snow && !state.racers[0].skier.down);
  run_for(state, fs::Controls{}, 8);
  CHECK(state.heli.body.spool == 0);
  const double x = state.heli.body.x, z = state.heli.body.z;
  run_for(state, fs::Controls{}, 2);
  CHECK(state.heli.body.x == x && state.heli.body.z == z);
}

TEST(crashed_it_throws_him_and_the_ride_starts_again_from_the_pad) {
  fs::GameState state = aloft(14);
  // The lever thrown down 20 m up: it falls onto its skids and crashes.
  const std::vector<fs::GameEvent> all = run_for(state, fs::Controls{}, 4);
  CHECK_MSG(said(all, fs::HeliPhase::crash) && state.heli.mode == fs::HeliMode::wreck,
            "a dropped lever 20 m up did not crash it; run `make lab LAB=heli "
            "ARGS=--scenario=drop-lever` (skill: helicopter)");
  CHECK(!fs::on_heli(state, 0) && state.heli.aboard);
  CHECK(state.racers[0].skier.down && state.racers[0].progress.wipeouts == 1);
  CHECK(!fs::heli_within(state, 0));
  bool again = false;
  for (int i = 0; i < static_cast<int>(10 * fs::kHz) && !again; i++) {
    again = said(run_for(state, fs::Controls{}, 1 / fs::kHz), fs::HeliPhase::restart);
  }
  CHECK(again && fs::on_heli(state, 0) && state.heli.mode == fs::HeliMode::flown);
  CHECK(state.heli.body.spool == 1 && !state.racers[0].skier.down);
  CHECK(std::hypot(state.heli.body.x - state.heli.pad_x, state.heli.body.z - state.heli.pad_z) <
        1e-9);
}

TEST(his_reset_on_the_skid_starts_the_ride_again_from_the_pad) {
  fs::GameState state = aloft(14);
  fs::Controls reset;
  reset.reset = true;
  CHECK(said(run_for(state, reset, 1 / fs::kHz), fs::HeliPhase::restart));
  CHECK(fs::on_heli(state, 0) && state.heli.body.grounded);
  CHECK(state.racers[0].progress.resets == 0);
}

TEST(the_tape_carries_the_lever_and_the_pedal) {
  fs::Controls c;
  c.lever = 0.6;
  c.pedal = -0.4;
  c.lean = 0.3;
  const fs::Controls s = fs::snap_controls(c);
  CHECK(std::abs(s.lever - 0.6) < 0.01 && std::abs(s.pedal + 0.4) < 0.01);
  CHECK(std::abs(s.lean - 0.3) < 0.01);
  const ge::helicopter::HeliControls h = fs::heli_controls(s);
  CHECK(h.collective == s.lever && h.pitch == -s.lean && h.pedal == s.pedal);
}

TEST(the_bot_flies_to_the_highest_run_on_seed_38_and_skis_down_and_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto level = std::make_shared<const mg::Level>(fs::free_map(kSeed, kVersion));
  // The pad clear of the trunks and of the snowmobile's spot.
  const fs::GameState parked = fs::create_free_ride(level);
  for (const mg::TreeDef& t : level->trees) {
    CHECK(std::hypot(t.x - parked.heli.pad_x, t.z - parked.heli.pad_z) > fs::heli_ride::kPadRadius);
  }
  CHECK(std::hypot(parked.sled.body.x - parked.heli.pad_x, parked.sled.body.z - parked.heli.pad_z) >
        fs::heli_ride::kPadRadius);
  fs::SimOptions opts;
  opts.max_seconds = 1800;
  const fs::RunReport r = fs::simulate_heli_outing(level, kSeed, opts);
  CHECK_MSG(r.finished && r.heli_rides == 1,
            "the bot's outing on the helicopter did not end at a foot; run `make lab LAB=free "
            "ARGS=--heli=1` and look at seed-38-heli.png (skill: sim)");
  CHECK_MSG(r.climbed > 800, "the helicopter lifted him only " + std::to_string(r.climbed) + " m");
  CHECK(r.heli_time > 60 && r.wipeouts == 0 && r.resets == 0);
  const std::string got = std::string(r.finished ? "1" : "0") + " " + std::to_string(r.heli_rides) +
                          " " + hex(ge::test::bits(r.time)) + " " + hex(ge::test::bits(r.climbed)) +
                          " " + hex(ge::test::bits(r.distance)) + " " + hex(r.digest);
#ifdef NDEBUG
  const fs::RunReport again = fs::simulate_heli_outing(level, kSeed, opts);
  CHECK_MSG(again.digest == r.digest, "the helicopter's outing is not a pure function of its map");
#endif
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# finished taken time-bits climbed-bits distance-bits digest "
           "(fall_line_heli_test, seed 38 at v10, the reference skis)\n"
        << got << "\n";
    return;
  }
  std::string want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    for (const auto& w : row) want += (want.empty() ? "" : " ") + w;
  }
  CHECK_MSG(got == want, "the helicopter's outing moved:\n  got  " + got + "\n  want " + want +
                             "\nIf the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1, and put the free lab's heli table before "
                             "and after in the PR.");
}
