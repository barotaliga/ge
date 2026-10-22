// The input log (engine/play/inputs.hpp): written and read back to the same runs and digest, a
// recorder merging equal steps into one run, hand-written values snapped onto their axes, and
// every malformed log refused with the line and what is wrong.
//
// ge:summary Tests: the input log's text round trip, its runs, snap_axis and its parse errors
// ge:keywords test play inputs input-log parse round-trip snap_axis rle make_setup
// ge:skill agent-interface testing
// ge:test tests/play_inputs_test.cpp
#include <array>
#include <limits>
#include <string>

#include "check.hpp"
#include "play/inputs.hpp"

namespace gp = ge::play;

namespace {

constexpr std::array<gp::Axis, 2> kAxes{{
    {"steer", gp::AxisKind::signed_unit, "left .. right"},
    {"fire", gp::AxisKind::press, "once"},
}};
constexpr std::array<gp::Option, 1> kOptions{{{"lanes", "3", "how many"}}};
const gp::GameSpec kSpec{"toy", "a toy", 60, 10, kAxes, kOptions, nullptr};

}  // namespace

TEST(axes_snap_onto_their_grids) {
  CHECK(gp::snap_axis(0.5, gp::AxisKind::signed_unit) == 64.0 / 127);
  CHECK(gp::snap_axis(-3, gp::AxisKind::signed_unit) == -1);
  CHECK(gp::snap_axis(-0.2, gp::AxisKind::lever) == 0);
  CHECK(gp::snap_axis(0.5, gp::AxisKind::lever) == 128.0 / 255);
  CHECK(gp::snap_axis(0.7, gp::AxisKind::toggle) == 1 &&
        gp::snap_axis(0.2, gp::AxisKind::press) == 0);
  CHECK(gp::snap_axis(std::numeric_limits<double>::quiet_NaN(), gp::AxisKind::signed_unit) == 0);
  CHECK(gp::snap_axis(gp::snap_axis(0.3, gp::AxisKind::lever), gp::AxisKind::lever) ==
        gp::snap_axis(0.3, gp::AxisKind::lever));
}

TEST(a_setup_fills_fallbacks_and_refuses_an_unknown_option) {
  gp::Setup s;
  std::string error;
  CHECK(gp::make_setup(kSpec, 7, {}, s, error) && s.options.at("lanes") == "3" &&
        s.number("lanes") == 3);
  CHECK(gp::make_setup(kSpec, 7, {{"lanes", "5"}}, s, error) && s.number("lanes") == 5);
  CHECK(!gp::make_setup(kSpec, 7, {{"lane", "5"}}, s, error));
  CHECK(error.find("it takes lanes") != std::string::npos);
}

TEST(a_log_round_trips_through_its_text) {
  gp::Setup s;
  std::string error;
  gp::make_setup(kSpec, 38.5, {}, s, error);
  gp::InputRecorder rec(gp::new_log(kSpec, s));
  const double steer = gp::snap_axis(0.3, gp::AxisKind::signed_unit);
  for (int i = 0; i < 5; i++) rec.record(std::array<double, 2>{0, 0});
  rec.record(std::array<double, 2>{steer, 1});
  for (int i = 0; i < 3; i++) rec.record(std::array<double, 2>{steer, 0});
  gp::InputLog log = rec.log();
  log.digest = 0x0123456789abcdefull;
  CHECK(log.runs.size() == 3 && log.steps() == 9);
  CHECK(log.at(5)[1] == 1 && log.at(6)[1] == 0 && log.at(9).empty());
  const std::string text = gp::to_text(log);
  CHECK(
      text.starts_with("ge-inputs 1\ngame toy\nseed 38.5\noption lanes 3\nhz 60\naxes steer fire\n"
                       "steps 9\ndigest 0123456789abcdef\n5 0 0\n"));
  const auto back = gp::parse_inputs(text, error);
  CHECK_MSG(back.has_value(), error);
  if (!back) return;
  CHECK(gp::to_text(*back) == text);
  CHECK(back->at(6)[0] == steer);
  CHECK(gp::fits(*back, kSpec, error));
}

TEST(a_hand_written_log_reads_with_comments_and_loose_spacing) {
  std::string error;
  const auto log = gp::parse_inputs(
      "# a run by hand\nge-inputs 1\ngame toy\nseed 3\naxes steer fire\n\n"
      "  120   0.5 0   # straight on\n1 -1 1\n",
      error);
  CHECK_MSG(log.has_value(), error);
  if (!log) return;
  CHECK(log->steps() == 121 && !log->digest && log->at(120)[0] == -1);
}

TEST(a_malformed_log_is_refused_with_its_line) {
  struct Case {
    const char* text;
    const char* says;
  };
  const Case cases[] = {
      {"", "starts with `ge-inputs 1`"},
      {"game toy\n", "line 1: expected `ge-inputs 1`"},
      {"ge-inputs 2\n", "a newer engine"},
      {"ge-inputs 1\ngame toy\n5 0 0\n", "line 3: a run of inputs before the `axes` line"},
      {"ge-inputs 1\ngame toy\naxes steer fire\n5 0\n", "holds 1 values; the log has 2 axes"},
      {"ge-inputs 1\ngame toy\naxes steer fire\n0.5 0 0\n", "whole number of steps"},
      {"ge-inputs 1\ngame toy\naxes steer fire\n5 x 0\n", "`x` is not a number"},
      {"ge-inputs 1\ngame toy\naxes steer fire\nsteps 4\n5 0 0\n", "says 4 steps; the runs hold 5"},
      {"ge-inputs 1\ngame toy\naxes steer fire\ndigest 12\n", "sixteen lower-case hex"},
      {"ge-inputs 1\naxes steer\n", "lacks its `game`"},
      {"ge-inputs 1\ngame toy\naxes a\nfly away\n", "line 4: unreadable line"},
  };
  for (const Case& c : cases) {
    std::string error;
    const auto log = gp::parse_inputs(c.text, error);
    CHECK_MSG(!log.has_value() && error.find(c.says) != std::string::npos,
              std::string(c.text) + " -> " + error);
  }
  std::string error;
  const auto other = gp::parse_inputs("ge-inputs 1\ngame toy\naxes steer\n", error);
  CHECK(other && !gp::fits(*other, kSpec, error) &&
        error.find("`steer fire`") != std::string::npos);
}
