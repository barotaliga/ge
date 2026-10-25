// The gyro's bias (engine/input/gyro.hpp): a pad at rest with a bias and a little noise settles
// it and reads near zero after; a turn neither moves the bias nor is taken for one; a slow steady
// turn never settles; an emptied slot forgets its bias.
//
// ge:summary Tests: the gyro's bias settles while still, keeps through a turn, never on a turn
// ge:keywords test input gyro bias still settle unbias_gyro
// ge:skill input testing
#include <cmath>

#include "check.hpp"
#include "input/gyro.hpp"
#include "input/sim.hpp"

namespace in = ge::input;

namespace {

constexpr double kDt = 1.0 / 60;
constexpr float kBias[3] = {0.02f, -0.03f, 0.01f};

// The pad's reading at frame `i`: the hand's turn, the bias and a small even noise.
void read(in::SimInput& sim, in::DeviceId pad, int i, float turn_y) {
  const float noise = (i % 2 ? 0.008f : -0.008f) * (i % 3 ? 1.0f : 0.5f);
  in::sim_gyro(sim, pad, kBias[0] + noise, kBias[1] + turn_y - noise, kBias[2] + noise);
}

// One frame stepped: the reading, the state, the bias.
const in::Slot& frame(in::SimInput& sim, in::InputState& s, in::GyroBiases& g, in::DeviceId pad,
                      int i, float turn_y) {
  read(sim, pad, i, turn_y);
  in::sim_advance(sim, kDt);
  in::sim_frame(sim, s);
  in::unbias_gyro(s, g, kDt);
  return *in::find_slot(s, pad);
}

}  // namespace

TEST(a_still_pad_settles_its_bias_and_reads_near_zero_after) {
  in::SimInput sim;
  in::InputState s;
  in::GyroBiases g;
  const auto pad = in::sim_plug(sim, in::sim_pad(in::Family::playstation));
  int i = 0;
  for (; i < 30; i++) frame(sim, s, g, pad, i, 0);  // half a second: not settled yet
  const in::Slot& early = *in::find_slot(s, pad);
  CHECK(std::fabs(early.gyro[1] - kBias[1]) < 0.02f);
  for (; i < 90; i++) frame(sim, s, g, pad, i, 0);
  const in::Slot& sl = frame(sim, s, g, pad, i++, 0);
  for (int a = 0; a < 3; a++) CHECK(std::fabs(sl.gyro[static_cast<size_t>(a)]) < 0.012f);
}

TEST(a_turn_keeps_the_bias_and_reads_as_the_hands_turn) {
  in::SimInput sim;
  in::InputState s;
  in::GyroBiases g;
  const auto pad = in::sim_plug(sim, in::sim_pad(in::Family::playstation));
  int i = 0;
  for (; i < 90; i++) frame(sim, s, g, pad, i, 0);
  for (int k = 0; k < 60; k++, i++) {
    const in::Slot& sl = frame(sim, s, g, pad, i, 0.5f);
    CHECK(std::fabs(sl.gyro[1] - 0.5f) < 0.012f);
  }
  const in::Slot& after = frame(sim, s, g, pad, i, 0);
  CHECK(std::fabs(after.gyro[1]) < 0.012f);
}

TEST(a_slow_steady_turn_is_never_taken_for_a_bias) {
  in::SimInput sim;
  in::InputState s;
  in::GyroBiases g;
  const auto pad = in::sim_plug(sim, in::sim_pad(in::Family::playstation));
  for (int i = 0; i < 180; i++) frame(sim, s, g, pad, i, 0.2f);
  CHECK(!g.slot[0].settled && !g.slot[1].settled);
}

TEST(an_emptied_slot_forgets_its_bias) {
  in::SimInput sim;
  in::InputState s;
  in::GyroBiases g;
  const auto pad = in::sim_plug(sim, in::sim_pad(in::Family::playstation));
  for (int i = 0; i < 90; i++) frame(sim, s, g, pad, i, 0);
  bool settled = false;
  for (const in::GyroBias& b : g.slot) settled = settled || b.settled;
  CHECK(settled);
  in::sim_unplug(sim, pad);
  in::sim_advance(sim, kDt);
  in::sim_frame(sim, s);
  in::unbias_gyro(s, g, kDt);
  settled = false;
  for (const in::GyroBias& b : g.slot) settled = settled || b.settled;
  CHECK(!settled);
}
