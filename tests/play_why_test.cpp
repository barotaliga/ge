// Why a bot did it (engine/play/why.hpp) over a toy game whose pilot records its decision: the
// replay with the trace on keeps the log's digest, the pilot asked at every step agrees with its
// own log and names the first step it would have played another, the timeline's columns and
// rows, the flips and the close calls, the `play why` command, the protocol's `why` op, and a
// game whose bots record nothing refused with a next step.
//
// ge:summary Tests: why_replay, why_timeline, why_flips, play why and the why op on a toy game
// ge:keywords test play why why_replay timeline flips close-call agrees protocol trace
// ge:skill bots agent-interface testing
// ge:test tests/play_why_test.cpp
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/num_text.hpp"
#include "lab/series.hpp"
#include "play/main.hpp"
#include "play/session.hpp"
#include "play/why.hpp"

namespace gp = ge::play;

namespace {

// A body on a line, over at x >= 10. Its pilot pushes while it is slower than 6 and coasts
// after, recording that as `throttle`; with `traced` false the Run refuses a trace.
class Toy final : public gp::Run {
 public:
  Toy(double seed, bool traced) : x_(seed), traced_(traced) {}
  void step(std::span<const double> in) override {
    v_ += in[0] * 0.5;
    x_ += v_ / 60;
    steps_++;
    hash_ = (hash_ ^ static_cast<uint64_t>(x_ * 1e6)) * 0x100000001b3ull;
  }
  bool over() const override { return x_ >= 10; }
  uint64_t digest() const override { return hash_; }
  std::string observe() override { return "{\"x\":" + ge::core::num::to_fixed(x_, 3) + "}"; }
  std::string state() const override { return "{\"x\":" + ge::core::num::to_string(x_) + "}"; }
  std::string report() const override { return "{}"; }
  bool pilot(std::span<double> out) override {
    out[0] = v_ < 6 ? 1 : 0;
    if (ge::ai::Why* w = why_ ? why_->follow(0, steps_) : nullptr) {
      w->decide("throttle");
      w->fact("speed", v_);
      w->option("push", 6 - v_);
      w->option("coast", 0);
      w->choose(out[0] > 0 ? "push" : "coast", out[0] > 0 ? "slow" : "fast_enough");
    }
    return true;
  }
  bool trace(ge::ai::Why* why) override {
    if (!traced_) return false;
    why_ = why;
    return true;
  }

 private:
  double x_, v_ = 0;
  int64_t steps_ = 0;
  bool traced_;
  ge::ai::Why* why_ = nullptr;
  uint64_t hash_ = 0xcbf29ce484222325ull;
};

constexpr std::array<gp::Axis, 1> kAxes{{{"push", gp::AxisKind::lever, "accelerate"}}};
constexpr std::array<gp::Option, 1> kOptions{{{"traced", "1", "whether its bot records"}}};
std::unique_ptr<gp::Run> open_toy(const gp::Setup& s, std::string&) {
  return std::make_unique<Toy>(s.seed, s.number("traced") != 0);
}
const gp::GameSpec kToy{"toy", "a body on a line", 60, 30, kAxes, kOptions, open_toy};
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

int play(std::vector<std::string> words, std::string& out, const std::string& in = "") {
  words.insert(words.begin(), "play");
  std::vector<char*> argv;
  for (std::string& w : words) argv.push_back(w.data());
  std::istringstream input(in);
  std::ostringstream output;
  const int code = gp::play_main(static_cast<int>(argv.size()), argv.data(), input, output);
  out = output.str();
  return code;
}

}  // namespace

TEST(the_traced_replay_keeps_the_logs_digest_and_the_pilot_agrees_with_its_own_log) {
  std::string error;
  const auto p = gp::pilot_run(kToy, setup(), error);
  CHECK(p.has_value());
  const auto w = gp::why_replay(kToy, p->log, 0, error);
  CHECK_MSG(w.has_value(), error);
  CHECK(w->digest == p->digest && w->steps == p->steps);
  CHECK(w->asked_pilot && w->agrees.size() == static_cast<size_t>(p->steps));
  for (const uint8_t a : w->agrees) CHECK(a == 1);
  CHECK(w->why.decisions().size() == static_cast<size_t>(p->steps));
  // The pilot pushes to 6 at half a unit a step: 12 pushes, then it coasts.
  CHECK(std::string(w->why.decisions()[11].chosen) == "push");
  CHECK(std::string(w->why.decisions()[12].chosen) == "coast");
  CHECK(w->why.decisions()[12].margin == 0);
}

TEST(on_another_players_log_it_says_where_the_pilot_would_have_played_otherwise) {
  std::string error;
  auto p = gp::pilot_run(kToy, setup(), error);
  CHECK(p.has_value());
  gp::InputLog log = p->log;
  // Hold the push two steps longer than the pilot did.
  log.runs.insert(log.runs.begin() + 1, gp::InputRun{2, {1}});
  log.digest.reset();
  const auto w = gp::why_replay(kToy, log, 0, error);
  CHECK(w.has_value());
  CHECK(w->agrees[11] == 1 && w->agrees[12] == 0);
}

TEST(the_timeline_is_one_row_a_step_with_the_choice_its_margin_and_its_facts) {
  std::string error;
  const auto p = gp::pilot_run(kToy, setup(), error);
  const auto w = gp::why_replay(kToy, p->log, 0, error);
  const ge::lab::Series s = gp::why_timeline(w->why, 0, kToy.hz);
  CHECK(s.rows() == static_cast<size_t>(p->steps));
  const ge::lab::Column* choice = s.find("throttle");
  CHECK(choice && choice->text && choice->cell[0] == "push" && choice->cell[20] == "coast");
  CHECK(s.find("throttle.choice")->num[20] == 1);
  CHECK(s.find("throttle.margin")->num[0] == 6);
  CHECK(s.find("throttle.speed")->num[1] == 0.5);
  CHECK(s.find("t_s")->num[60] == 1);
  std::string back_error;
  ge::lab::Series back;
  CHECK_MSG(ge::lab::read_series(ge::lab::write_series(s), back, back_error), back_error);
  CHECK(gp::why_timeline(w->why, 3, kToy.hz).rows() == 0);
}

TEST(the_flips_are_the_changes_of_mind_and_the_close_calls_under_a_margin) {
  std::string error;
  const auto p = gp::pilot_run(kToy, setup(), error);
  const auto w = gp::why_replay(kToy, p->log, 0, error);
  const auto flips = gp::why_flips(w->why, 0, 0);
  CHECK(flips.size() == 1);
  CHECK(flips[0].step == 12 && std::string(flips[0].from) == "push" &&
        std::string(flips[0].to) == "coast");
  // Margins under 1: the last push (6 - 5.5) and every coast (margin 0).
  const auto close = gp::why_flips(w->why, 0, 1);
  CHECK(close.size() > flips.size());
  CHECK(close[0].step == 11 && std::string(close[0].from) == std::string(close[0].to));
}

TEST(play_why_answers_the_decisions_round_a_step_its_flips_and_its_timeline) {
  const auto dir = std::filesystem::temp_directory_path() / "ge_play_why_test";
  std::filesystem::remove_all(dir);
  std::string out;
  CHECK(play({"sim", "--out=" + dir.string()}, out) == 0);
  const std::string log = (dir / "toy-1.inputs").string();
  CHECK_MSG(play({"why", log, "--step=12", "--around=1", "--flips", "--timeline",
                  "--out=" + dir.string()},
                 out) == 0,
            out);
  CHECK_MSG(has(out, "\"from\":11,\"to\":13") && has(out, "\"pilot_asked\":true") &&
                has(out, "\"first_disagree\":-1") && has(out, "\"chosen\":\"coast\""),
            out);
  CHECK_MSG(has(out, "\"flips\":[{\"step\":12,\"decision\":\"throttle\",\"from\":\"push\""), out);
  CHECK_MSG(has(out, "step 12 agent 0 throttle: coast"), out);
  CHECK(std::filesystem::file_size(dir / "toy-1-why-0.tsv") > 0);
  CHECK(play({"why"}, out) == 2 && has(out, "\"next\""));
  CHECK(play({"why", log, "--agent=-1"}, out) == 2);
  std::filesystem::remove_all(dir);
}

TEST(the_protocols_why_says_what_the_pilot_would_do_now_without_stepping) {
  std::string out;
  CHECK(play({}, out, "{\"op\":\"why\"}\n{\"op\":\"observe\"}\n") == 0);
  CHECK_MSG(has(out, "\"ops\":[\"observe\",\"act\",\"pilot\",\"why\""), out);
  CHECK_MSG(has(out, "{\"op\":\"why\",\"step\":0,\"pilot\":{\"push\":1},\"decisions\":[{"), out);
  CHECK_MSG(has(out, "\"chosen\":\"push\""), out);
  CHECK_MSG(has(out, "{\"op\":\"observe\",\"step\":0"), out);
}

TEST(a_game_whose_bots_record_nothing_is_refused_with_a_next_step) {
  std::string error;
  const auto p = gp::pilot_run(kToy, setup("0"), error);
  CHECK(p.has_value());
  CHECK(!gp::why_replay(kToy, p->log, 0, error) && has(error, "record no decisions"));
  gp::Session s(kToy, setup("0"));
  CHECK(has(s.handle("{\"op\":\"why\"}"), "\"next\""));
}
