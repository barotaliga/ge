// The play session (engine/play/session.hpp, main.hpp, json_read.hpp) over a toy game: the
// protocol's ops and refusals, a press taken once per hold, the pilot to the end, a log replayed
// to its digest, the state at a step, two logs diverging at the input that differs, a still of
// the run at a step, the `play` command line's answers and exit codes, and the flat JSON reader.
//
// ge:summary Tests: the play protocol, pilot, replay, state, diverge and play_main on a toy game
// ge:keywords test play session protocol pilot replay state diverge play_main json read_flat shot
// ge:skill agent-interface testing
// ge:test tests/play_session_test.cpp
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/num_text.hpp"
#include "lab/raster.hpp"
#include "play/bench.hpp"
#include "play/json_read.hpp"
#include "play/main.hpp"
#include "play/session.hpp"
#include "prof/zones.hpp"

namespace gp = ge::play;

namespace {

// A body on a line: `push` accelerates it, `kick` (a press) adds a step of speed once, and the
// run is over at x >= 10. Its pilot pushes until it is moving, then coasts.
class Toy final : public gp::Run {
 public:
  explicit Toy(double seed) : x_(seed) {}
  void step(std::span<const double> in) override {
    GE_ZONE("toy.move");
    v_ += in[0] * 0.5 + in[1];
    x_ += v_ / 60;
    kicks_ += in[1] != 0 ? 1 : 0;
    hash_ = (hash_ ^ static_cast<uint64_t>(x_ * 1e6)) * 0x100000001b3ull;
  }
  bool over() const override { return x_ >= 10; }
  uint64_t digest() const override { return hash_; }
  std::string observe() override { return "{\"x\":" + ge::core::num::to_fixed(x_, 3) + "}"; }
  std::string state() const override {
    return "{\"x\":" + ge::core::num::to_string(x_) + ",\"v\":" + ge::core::num::to_string(v_) +
           ",\"body\":{\"kicks\":" + std::to_string(kicks_) + "},\"list\":[1,2]}";
  }
  std::string report() const override { return "{\"kicks\":" + std::to_string(kicks_) + "}"; }
  bool pilot(std::span<double> out) override {
    out[0] = v_ < 6 ? 1 : 0;
    return true;
  }
  // One view: the line, 12 m long, the body a white mark on it.
  bool shoot(std::string_view view, ge::lab::Picture& out, std::string& caption,
             std::string& error) override {
    if (view != "line") {
      error = "no such view";
      return false;
    }
    out = ge::lab::create_picture(120, 20, {0, 0, 0});
    ge::lab::fill_rect(out, x_ * 10, 0, 2, 20, {255, 255, 255});
    caption = "the body at x " + ge::core::num::to_fixed(x_, 2);
    return true;
  }

 private:
  double x_, v_ = 0;
  int kicks_ = 0;
  uint64_t hash_ = 0xcbf29ce484222325ull;
};

std::unique_ptr<gp::Run> open_toy(const gp::Setup& s, std::string& error) {
  if (s.number("lanes") > 4) {
    error = "lanes must be 4 or fewer";
    return nullptr;
  }
  return std::make_unique<Toy>(s.seed);
}

constexpr std::array<gp::Axis, 2> kAxes{{
    {"push", gp::AxisKind::lever, "accelerate"},
    {"kick", gp::AxisKind::press, "a step of speed, once"},
}};
constexpr std::array<gp::Option, 1> kOptions{{{"lanes", "1", "how many"}}};
constexpr std::array<gp::View, 1> kViews{{{"line", "the line from above"}}};
const gp::GameSpec kToy{"toy", "a body on a line", 60, 30, kAxes, kOptions, open_toy, kViews};
GE_GAME(kToy);

gp::Setup setup(double seed = 0) {
  gp::Setup s;
  std::string error;
  gp::make_setup(kToy, seed, {}, s, error);
  return s;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

int play(std::vector<std::string> words, std::string& out, const std::string& in = "") {
  words.insert(words.begin(), "play");
  std::vector<char*> argv;
  argv.reserve(words.size());
  for (std::string& w : words) argv.push_back(w.data());
  std::istringstream input(in);
  std::ostringstream output;
  const int code = gp::play_main(static_cast<int>(argv.size()), argv.data(), input, output);
  out = output.str();
  return code;
}

}  // namespace

TEST(the_protocol_answers_every_op_and_takes_a_press_once_a_hold) {
  gp::Session s(kToy, setup());
  CHECK(s.ok());
  CHECK(has(s.hello(), "\"game\":\"toy\"") && has(s.hello(), "\"kind\":\"press\""));
  const std::string act =
      s.handle("{\"op\":\"act\",\"input\":{\"push\":1,\"kick\":true},\"steps\":3}");
  CHECK_MSG(has(act, "\"op\":\"act\",\"step\":3,") && has(act, "\"x\":"), act);
  CHECK(has(s.handle("{\"op\":\"state\"}"), "\"kicks\":1"));
  CHECK(has(s.handle("{\"op\":\"inputs\"}"), "1 1 1\\n2 1 0\\n"));
  CHECK(has(s.handle("{\"op\":\"pilot\",\"steps\":10}"), "\"step\":13"));
  CHECK(has(s.handle("{\"op\":\"act\",\"input\":{\"jump\":1}}"), "the axes push, kick"));
  const std::string end = s.handle("{\"op\":\"end\"}");
  CHECK(has(end, "\"op\":\"end\",\"steps\":13,\"digest\":\"") && has(end, "\"kicks\":1"));
}

TEST(a_setup_the_game_refuses_says_why) {
  gp::Setup s = setup();
  s.options["lanes"] = "9";
  gp::Session bad(kToy, s);
  CHECK(!bad.ok() && bad.error() == "lanes must be 4 or fewer");
}

TEST(the_pilot_plays_to_the_end_and_its_log_replays) {
  std::string error;
  const auto p = gp::pilot_run(kToy, setup(1), error);
  CHECK_MSG(p.has_value(), error);
  if (!p) return;
  CHECK(p->over && p->steps > 0 && p->log.digest == p->digest);
  const auto again = gp::replay(kToy, p->log, error);
  CHECK(again && again->digest == p->digest && again->steps == p->steps);
  const auto st = gp::state_at(kToy, p->log, 0, error);
  CHECK(st && has(*st, "\"x\":1,"));
  CHECK(!gp::state_at(kToy, p->log, p->steps + 1, error) && has(error, "outside the log"));
}

TEST(diverge_names_the_first_input_and_the_first_state_that_differ) {
  std::string error;
  const auto p = gp::pilot_run(kToy, setup(), error);
  if (!p) return;
  gp::InputLog b = p->log;
  b.runs.front().count -= 1;
  b.runs.insert(b.runs.begin() + 1,
                gp::InputRun{1, {1, 1}});  // a kick a step from the end of run 0
  b.digest.reset();
  const int64_t at = b.runs.front().count;
  const auto d = gp::diverge(kToy, p->log, b, error);
  CHECK_MSG(d.has_value(), error);
  if (!d) return;
  CHECK(!d->same && d->input_step == at && d->state_step == at + 1);
  bool kicks = false;
  for (const gp::FieldDiff& f : d->fields) kicks = kicks || (f.key == "body.kicks" && f.b == "1");
  CHECK_MSG(kicks, "diverge does not name body.kicks");
  gp::InputLog seeded = p->log;
  seeded.seed = 2;
  const auto s = gp::diverge(kToy, p->log, seeded, error);
  CHECK(s && s->setup_differs && s->state_step == 0 && !s->same);
}

TEST(the_play_command_line_answers_one_json_line_with_its_exit_code) {
  const auto dir = std::filesystem::temp_directory_path() / "ge_play_session_test";
  std::filesystem::remove_all(dir);
  std::string out;
  CHECK(play({"describe"}, out) == 0 && has(out, "\"game\":\"toy\"") &&
        has(out, "\"default\":\"1\""));
  CHECK(play({"sim", "--seeds=0,1", "--out=" + dir.string()}, out) == 0);
  CHECK_MSG(has(out, "\"runs\":[{\"seed\":0,") && has(out, "{\"seed\":1,"), out);
  const std::string log = (dir / "toy-0.inputs").string();
  CHECK(play({"replay", log}, out) == 0 && has(out, "\"matches\":true"));
  CHECK(play({"state", log, "--step=2"}, out) == 0 && has(out, "\"step\":2,\"state\":{"));
  CHECK(play({"diverge", log, log}, out) == 0 && has(out, "\"same\":true"));
  CHECK(play({"diverge", log, (dir / "toy-1.inputs").string()}, out) == 1 &&
        has(out, "\"setup_differs\":true"));
  // A log whose digest no longer matches its inputs: the replay says so and exits 1.
  std::string text;
  {
    std::ifstream f(log);
    std::stringstream ss;
    ss << f.rdbuf();
    text = ss.str();
  }
  const size_t d = text.find("digest ");
  text.replace(d + 7, 16, "0000000000000000");
  std::ofstream(dir / "bad.inputs") << text;
  CHECK(play({"replay", (dir / "bad.inputs").string()}, out) == 1 && has(out, "\"matches\":false"));
  CHECK(play({"replay"}, out) == 2 && has(out, "\"next\":"));
  CHECK(play({"fly"}, out) == 2 && has(out, "unknown command"));
  CHECK(play({"sim", "--wings=2"}, out) == 2 && has(out, "has no option wings"));
  CHECK(play({"--seed=0", "--inputs=" + (dir / "proto.inputs").string()}, out,
             "{\"op\":\"act\",\"input\":{\"push\":1},\"steps\":4}\n{\"op\":\"end\"}\n") == 0);
  CHECK(has(out, "\"op\":\"hello\"") && has(out, "\"op\":\"end\",\"steps\":4"));
  CHECK(play({"replay", (dir / "proto.inputs").string()}, out) == 0 && has(out, "\"steps\":4"));
  std::filesystem::remove_all(dir);
}

TEST(the_flat_reader_reads_dotted_paths_and_refuses_what_it_should) {
  gp::FlatJson m;
  std::string error;
  CHECK(gp::read_flat(" {\"a\":{\"b\":-1.5e2,\"c\":\"x\\\"y\"},\"d\":true,\"e\":null} ", m, error));
  CHECK(m.at("a.b").number == -150 && m.at("a.b").text == "-1.5e2" && m.at("a.c").text == "x\"y");
  CHECK(m.at("d").boolean && m.at("e").kind == gp::JsonValue::Kind::null);
  CHECK(!gp::read_flat("{\"a\":[1]}", m, error) && has(error, "an array"));
  CHECK(gp::read_flat("{\"a\":[1,{\"b\":2}]}", m, error, true) && m.at("a.1.b").number == 2);
  CHECK(gp::json_text(m.at("a.0")) == "1");
  CHECK(!gp::read_flat("{\"a\":1} x", m, error) && has(error, "column 9"));
}

TEST(the_bench_times_every_step_of_the_pilots_run_and_folds_its_zones) {
  std::string error;
  const auto p = gp::pilot_run(kToy, setup(1), error);
  CHECK_MSG(p.has_value(), error);
  if (!p) return;
  gp::BenchOptions o;
  o.repeats = 2;
  o.window = 10;
  const auto b = gp::bench(kToy, p->log, o, error);
  CHECK_MSG(b.has_value(), error);
  if (!b) return;
  CHECK(b->steps == p->steps && b->same_digest && b->digest == p->digest);
  CHECK(b->step_us.size() == static_cast<size_t>(b->steps) && b->dropped_spans == 0);
  CHECK(b->zones.size() == 1 && b->zones[0].name == "toy.move" && b->zones[0].calls_per_step == 1);
  CHECK(b->warmup_steps == 60 && b->steps_allocating == 0 && b->first_allocating == -1);
  // The toy's one zone is the only one a slow step can name, but on a coarse clock its time in that
  // step can read 0 and the step then names none: which of the two is the timer's, not the code's.
  CHECK(!b->slowest.empty() && (b->slowest[0].zone == "toy.move" || b->slowest[0].zone.empty()));
  CHECK(has(b->series, "step\tstep_us\tallocs\ttoy.move_us\n0\t"));
  CHECK(has(b->spans, "name\tdepth\tstart_ns\tdur_ns\ntoy.move\t0\t"));
  // Only the first steps of the log, and the digest is not the log's to check then.
  o.most_steps = 5;
  const auto few = gp::bench(kToy, p->log, o, error);
  CHECK(few && few->steps == 5 && few->same_digest);
  CHECK(gp::quantile({3, 1, 2}, 0.5) == 2 && gp::quantile({}, 0.5) == 0);
}

TEST(play_bench_answers_in_one_line_and_writes_its_files) {
  const auto dir = std::filesystem::temp_directory_path() / "ge_play_bench_test";
  std::filesystem::remove_all(dir);
  std::string out;
  CHECK(play({"bench", "--seeds=0", "--repeats=1", "--out=" + dir.string()}, out) == 0);
  CHECK_MSG(has(out, "\"game\":\"toy\"") && has(out, "\"same_digest\":true") &&
                has(out, "\"name\":\"toy.move\"") && has(out, "\"counting\":true"),
            out);
  CHECK(std::filesystem::exists(dir / "toy-0.bench.tsv") &&
        std::filesystem::exists(dir / "toy-0.spans.tsv"));
  CHECK(play({"bench", "--repeats=0"}, out) == 2 && has(out, "--repeats"));
  std::filesystem::remove_all(dir);
}

TEST(a_still_is_the_view_at_its_step_framed_with_its_words) {
  std::string error;
  const auto p = gp::pilot_run(kToy, setup(1), error);
  if (!p) return;
  const auto st = gp::shot_at(kToy, p->log, 30, "", error);
  CHECK_MSG(st.has_value(), error);
  if (!st) return;
  // The default view is the first; the picture carries its caption band under the view.
  CHECK(st->view == "line" && st->step == 30 && has(st->caption, "the body at x "));
  CHECK(st->picture.width >= 120 && st->picture.height > 20);
  const auto again = gp::state_at(kToy, p->log, 30, error);
  const auto replayed = gp::replay(kToy, p->log, error);
  CHECK(again && replayed);
  CHECK(!gp::shot_at(kToy, p->log, 30, "side", error) && has(error, "the views are line"));
  CHECK(!gp::shot_at(kToy, p->log, p->steps + 1, "line", error) && has(error, "outside"));
}

TEST(play_shot_writes_a_png_and_answers_where) {
  const auto dir = std::filesystem::temp_directory_path() / "ge_play_shot_test";
  std::filesystem::remove_all(dir);
  std::string out;
  CHECK(play({"shot", "--seed=1", "--step=20", "--out=" + dir.string()}, out) == 0);
  CHECK_MSG(has(out, "\"view\":\"line\"") && has(out, "\"step\":20") &&
                has(out, "\"caption\":\"the body at x "),
            out);
  CHECK(std::filesystem::file_size(dir / "toy-1-line-20.png") > 0);
  CHECK(play({"describe"}, out) == 0 && has(out, "\"views\":[{\"name\":\"line\""));
  CHECK(play({"shot", "--seed=1", "--view=side"}, out) == 1 && has(out, "the views are line"));
  std::filesystem::remove_all(dir);
}
