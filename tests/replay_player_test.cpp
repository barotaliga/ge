// The replay's player (engine/replay/player.hpp) over a toy game whose state is a stream of
// draws: a seek to any step, back or forward, in any budget, has the digest and the state of the
// run played straight there; a seek behind the first copy reopens from the setup; a seek ahead
// past a copy starts from it; a log of another game is refused with what differs.
//
// ge:summary Tests: the replay player's seek lands on the run played straight there
// ge:keywords test replay player seek advance budget restore reopen digest
// ge:skill replay testing
// ge:test tests/replay_player_test.cpp
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/num_text.hpp"
#include "core/prng.hpp"
#include "play/inputs.hpp"
#include "replay/player.hpp"

namespace gp = ge::play;
namespace gr = ge::replay;

namespace {

// A walker pushed by its one axis and jostled by its own seeded stream, so a copy that lost the
// stream's place would part from the straight run at once.
class Walker final : public gp::Run {
 public:
  explicit Walker(double seed) : rng_(ge::core::make_rng(static_cast<uint32_t>(seed))) {}
  void step(std::span<const double> in) override {
    x_ += in[0] + ge::core::next(rng_) - 0.5;
    hash_ = (hash_ ^ static_cast<uint64_t>(x_ * 1e9)) * 0x100000001b3ull;
  }
  bool over() const override { return false; }
  uint64_t digest() const override { return hash_; }
  std::string observe() override { return "{}"; }
  std::string state() const override { return "{\"x\":" + ge::core::num::to_string(x_) + "}"; }
  std::string report() const override { return "{}"; }
  std::unique_ptr<gp::Run> clone() const override { return std::make_unique<Walker>(*this); }

 private:
  ge::core::Rng rng_;
  double x_ = 0;
  uint64_t hash_ = 0xcbf29ce484222325ull;
};

constexpr std::array<gp::Axis, 1> kAxes{{{"push", gp::AxisKind::signed_unit, "the push"}}};

std::unique_ptr<gp::Run> open_walker(const gp::Setup& setup, std::string&) {
  return std::make_unique<Walker>(setup.seed);
}

const gp::GameSpec kWalker{"walker", "a walker", 10, 100, kAxes, {}, open_walker};

// 300 steps (30 s at 10 Hz) of pushes changing every few steps.
gp::InputLog walk_log() {
  gp::Setup setup;
  std::string error;
  gp::make_setup(kWalker, 7, {}, setup, error);
  gp::InputRecorder rec(gp::new_log(kWalker, setup));
  for (int i = 0; i < 300; i++) {
    const double v = gp::snap_axis(((i / 7) % 5 - 2) * 0.3, gp::AxisKind::signed_unit);
    rec.record(std::span<const double>(&v, 1));
  }
  return rec.log();
}

// The digest after `steps` steps of `log` played straight.
uint64_t straight(const gp::InputLog& log, int64_t steps) {
  Walker w(7);
  for (int64_t i = 0; i < steps; i++) w.step(log.at(i));
  return w.digest();
}

// At 10 Hz: a copy every 1 s, all of the last 5 s, one in 2 back to 10 s, one in 4 after.
const gr::KeepPolicy kPolicy{1, 5, 2, 10, 4, 0};

}  // namespace

TEST(every_seek_lands_on_the_straight_run) {
  const gp::InputLog log = walk_log();
  gr::Player p(kWalker, log, kPolicy);
  CHECK(p.ok());
  CHECK(p.length() == 300);
  const std::vector<int64_t> marks{300, 17, 250, 251, 0, 299, 3, 120, 80, 300, 45};
  for (int64_t m : marks) {
    p.seek(m);
    while (!p.advance(13)) {
    }
    CHECK_MSG(p.at() == m && p.run().digest() == straight(log, m),
              "seek to " + std::to_string(m) + " parted from the straight run");
  }
  CHECK(p.stats().restores > 0);
}

TEST(where_a_seek_starts) {
  const gp::InputLog log = walk_log();
  gr::Player p(kWalker, log, kPolicy);
  p.seek(300);
  CHECK(p.advance(1000));
  CHECK(p.stats().steps == 300 && p.stats().restores == 0);
  // At step 300 the copies are 0, 40 .. 160, 200 .. 240 by 20 and 250 .. 300 by 10: back to 287
  // is the copy at 280 and seven steps.
  p.seek(287);
  CHECK(p.advance(1000));
  CHECK(p.stats().restores == 1);
  CHECK(p.stats().steps == 307);
  // Behind every copy but the first: from the copy of the run as opened.
  p.seek(2);
  CHECK(p.advance(1000) && p.run().digest() == straight(log, 2));
}

TEST(a_budget_pays_a_long_seek_out) {
  const gp::InputLog log = walk_log();
  gr::Player p(kWalker, log, kPolicy);
  p.seek(200);
  int frames = 0;
  while (!p.advance(25)) frames++;
  CHECK(frames == 7);  // 200 steps at 25 a frame: seven short frames, then the one that lands
  CHECK(p.run().digest() == straight(log, 200));
}

TEST(a_log_of_another_game_is_refused) {
  gp::InputLog log = walk_log();
  log.game = "other";
  gr::Player p(kWalker, log, kPolicy);
  CHECK(!p.ok() && !p.error().empty());
  gr::Player bad(kWalker, walk_log(), {0.15, 5, 3, 10, 6, 0});  // 1.5 steps
  CHECK(!bad.ok() && bad.error().find("keep policy") != std::string::npos);
}
