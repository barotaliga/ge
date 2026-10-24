// What the replay tests share: a toy game whose state is a stream of draws (a walker pushed by
// its one axis and jostled by its own seeded stream, so a copy that lost the stream's place parts
// from the straight run at once), a 30 s log of it, the digest of that log played straight, and
// a keep policy small enough to reason about by hand.
//
// ge:summary Replay test support: the walker toy game, its log, the straight digest, a policy
// ge:keywords test replay walker toy-game straight digest policy opens
// ge:skill replay testing
#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>

#include "core/num_text.hpp"
#include "core/prng.hpp"
#include "play/game.hpp"
#include "play/inputs.hpp"
#include "replay/keep.hpp"

namespace replay_test {

namespace gp = ge::play;
namespace gr = ge::replay;

// Runs the walker's spec has opened (a clone is not an open).
inline int g_opens = 0;

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

inline constexpr std::array<gp::Axis, 1> kAxes{{{"push", gp::AxisKind::signed_unit, "the push"}}};

inline std::unique_ptr<gp::Run> open_walker(const gp::Setup& setup, std::string&) {
  g_opens++;
  return std::make_unique<Walker>(setup.seed);
}

inline const gp::GameSpec kWalker{"walker", "a walker", 10, 100, kAxes, {}, open_walker};

// The push at step `i`: changing every few steps.
inline double push_at(int i) {
  return gp::snap_axis(((i / 7) % 5 - 2) * 0.3, gp::AxisKind::signed_unit);
}

// 300 steps (30 s at 10 Hz) of `push_at`.
inline gp::InputLog walk_log() {
  gp::Setup setup;
  std::string error;
  gp::make_setup(kWalker, 7, {}, setup, error);
  gp::InputRecorder rec(gp::new_log(kWalker, setup));
  for (int i = 0; i < 300; i++) {
    const double v = push_at(i);
    rec.record(std::span<const double>(&v, 1));
  }
  return rec.log();
}

// The digest after `steps` steps of `log` played straight.
inline uint64_t straight(const gp::InputLog& log, int64_t steps) {
  Walker w(7);
  for (int64_t i = 0; i < steps; i++) w.step(log.at(i));
  return w.digest();
}

// At 10 Hz: a copy every 1 s, all of the last 5 s, one in 2 back to 10 s, one in 4 after.
inline const gr::KeepPolicy kPolicy{1, 5, 2, 10, 4, 0};

}  // namespace replay_test
