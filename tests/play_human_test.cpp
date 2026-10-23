// How human a run looks (engine/play/human.hpp) over a toy game: its pilot wants a bang-bang
// stick and changes its mind about the line every half second, recording what it wants as a
// "hands" decision and pressing it 0.2 s late. The report takes the stick and the lever, leaves
// the button out, measures the lag against the recorded intent, leaves "hands" out of the
// decisions, flags the saturated stick and the flipping line against a person's bands, and
// `play sim --human` writes it beside the run; a game whose bots record nothing is refused.
//
// ge:summary Tests: human_report and play sim --human on a toy game: axes, lag, decisions, flags
// ge:keywords test play human humanness report lag saturated flips out-of-band sim
// ge:skill bots agent-interface testing
// ge:test tests/play_human_test.cpp
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ai/hands.hpp"
#include "check.hpp"
#include "play/human.hpp"
#include "play/json_read.hpp"
#include "play/main.hpp"
#include "play/session.hpp"
#include "play/why.hpp"

namespace gp = ge::play;

namespace {

constexpr double kHz = 60;
constexpr int64_t kLate = 12;  // 0.2 s at 60 Hz
constexpr int64_t kSwing = 30;

// What the pilot wants at step `n`: hard left, then hard right, every half second.
double wanted(int64_t n) { return n < 0 ? 0 : ((n / kSwing) % 2 ? 1 : -1); }

// A run of 20 s, over at its last step. Axes: steer (a stick), push (a lever), hop (a button).
class Toy final : public gp::Run {
 public:
  explicit Toy(bool traced) : traced_(traced) {}
  void step(std::span<const double> in) override {
    hash_ = (hash_ ^ static_cast<uint64_t>((in[0] + 2) * 1e6)) * 0x100000001b3ull;
    steps_++;
  }
  bool over() const override { return steps_ >= 20 * static_cast<int64_t>(kHz); }
  uint64_t digest() const override { return hash_; }
  std::string observe() override { return "{\"step\":" + std::to_string(steps_) + "}"; }
  std::string state() const override { return observe_const(); }
  std::string report() const override { return "{}"; }
  bool pilot(std::span<double> out) override {
    out[0] = wanted(steps_ - kLate);
    out[1] = 0.5;
    out[2] = 0;
    if (ge::ai::Why* w = why_ ? why_->follow(0, steps_) : nullptr) {
      w->decide("line");
      w->option("left", wanted(steps_) < 0 ? 1 : 0);
      w->option("right", wanted(steps_) > 0 ? 1 : 0);
      w->choose(wanted(steps_) < 0 ? "left" : "right", "the swing");
      constexpr std::array<const char*, 2> kNames{"steer", "push"};
      const std::array<double, 2> want{wanted(steps_), 0.5};
      ge::ai::record_intent(w, kNames, want);
    }
    return true;
  }
  bool trace(ge::ai::Why* why) override {
    if (!traced_) return false;
    why_ = why;
    return true;
  }

 private:
  std::string observe_const() const { return "{\"step\":" + std::to_string(steps_) + "}"; }
  bool traced_;
  int64_t steps_ = 0;
  ge::ai::Why* why_ = nullptr;
  uint64_t hash_ = 0xcbf29ce484222325ull;
};

constexpr std::array<gp::Axis, 3> kAxes{{{"steer", gp::AxisKind::signed_unit, "turn"},
                                         {"push", gp::AxisKind::lever, "accelerate"},
                                         {"hop", gp::AxisKind::press, "jump"}}};
constexpr std::array<gp::Option, 1> kOptions{{{"traced", "1", "whether its bot records"}}};
std::unique_ptr<gp::Run> open_toy(const gp::Setup& s, std::string&) {
  return std::make_unique<Toy>(s.number("traced") != 0);
}
const gp::GameSpec kToy{"toy", "a stick swung side to side", kHz, 30, kAxes, kOptions, open_toy};
GE_GAME(kToy);

gp::Setup setup(const char* traced = "1") {
  gp::Setup s;
  std::string error;
  gp::make_setup(kToy, 0, {{"traced", traced}}, s, error);
  return s;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

int play(std::vector<std::string> words, std::string& out) {
  words.insert(words.begin(), "play");
  std::vector<char*> argv;
  argv.reserve(words.size());
  for (std::string& w : words) argv.push_back(w.data());
  std::istringstream input;
  std::ostringstream output;
  const int code = gp::play_main(static_cast<int>(argv.size()), argv.data(), input, output);
  out = output.str();
  return code;
}

}  // namespace

TEST(the_report_takes_the_stick_and_the_lever_measures_the_lag_and_flags_what_is_off) {
  std::string error;
  const auto p = gp::pilot_run(kToy, setup(), error);
  CHECK_MSG(p.has_value(), error);
  if (!p) return;
  const auto w = gp::why_replay(kToy, p->log, 0, error);
  CHECK_MSG(w.has_value(), error);
  if (!w) return;
  const gp::HumanReport r = gp::human_report(kToy, p->log, w->why);
  CHECK(r.axes.size() == 2);
  if (r.axes.size() != 2) return;
  CHECK(r.axes[0].first == "steer" && r.axes[1].first == "push");
  CHECK(std::fabs(r.axes[0].second.lag_s - static_cast<double>(kLate) / kHz) < 1e-9);
  CHECK(r.axes[0].second.saturated > 0.9);
  CHECK(r.decisions.size() == 1);
  if (r.decisions.empty()) return;
  CHECK(r.decisions[0].first == "line");
  CHECK(std::fabs(r.decisions[0].second.flips_min - 120) < 5);
  bool saturated = false, flips = false, lag = false;
  for (const ge::ai::OutOfBand& o : r.out) {
    saturated = saturated || (o.axis == "steer" && std::string(o.metric) == "saturated");
    flips = flips || (o.axis == "line" && std::string(o.metric) == "flips_min");
    lag = lag || std::string(o.metric) == "lag_s";
  }
  CHECK(saturated && flips && !lag);
  gp::FlatJson m;
  CHECK_MSG(gp::read_flat(gp::human_json(r), m, error, true), error);
  CHECK(m.contains("axes.1.lag_s") && m.contains("decisions.0.flips_min"));
  CHECK(m.contains("out_of_band.0") && !m.contains("axes.2.name"));
}

TEST(play_sim_human_writes_the_report_beside_each_run_and_a_silent_bot_is_refused) {
  const auto dir = std::filesystem::temp_directory_path() / "ge_play_human_test";
  std::filesystem::remove_all(dir);
  std::string out;
  CHECK_MSG(play({"sim", "--human", "--out=" + dir.string()}, out) == 0, out);
  CHECK_MSG(has(out, "\"human\":{\"axes\":[{\"name\":\"steer\"") && has(out, "steer saturated"),
            out);
  CHECK(play({"sim", "--human", "--traced=0", "--out=" + dir.string()}, out) == 1);
  CHECK_MSG(has(out, "\"next\""), out);
  CHECK(play({"sim", "--out=" + dir.string()}, out) == 0 && !has(out, "\"human\""));
  std::filesystem::remove_all(dir);
}
