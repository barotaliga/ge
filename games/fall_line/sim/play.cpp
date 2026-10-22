// Fall Line's Run on the play contract (play.hpp): the Driver behind it, the observation, the
// state, the report and the bot as the pilot.
//
// ge:summary FallLineRun: the Driver behind the play contract; observation, state, report, pilot
// ge:keywords play run observation state report pilot driver events
// ge:skill sim agent-interface
// ge:test tests/fall_line_play_test.cpp
#include "fall_line/sim/play.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/course.hpp"
#include "fall_line/sim/field.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/query.hpp"
#include "play/inputs.hpp"
#include "play/session.hpp"

namespace fall_line::sim {

namespace {

namespace mg = ge::mountain::mapgen;
namespace core = ge::core;
namespace num = ge::core::num;
using ge::play::quote;

std::string fixed(double v) { return num::to_fixed(v, 3); }
std::string exact(double v) { return num::to_string(v); }
std::string flag(bool b) { return b ? "true" : "false"; }

class FallLineRun final : public ge::play::Run {
 public:
  FallLineRun(std::shared_ptr<const mg::Level> level, double seed, int32_t rivals)
      : driver_(std::move(level), rivals), seed_(seed) {
    backlog_.reserve(kMostEvents);
  }

  void step(std::span<const double> in) override {
    Controls c;
    c.ski.steer = in[0];
    c.ski.tuck = in[1];
    c.ski.brake = in[2];
    c.ski.jump = in[3] != 0;
    c.reset = in[4] != 0;
    driver_.step(c);
    collect();
  }

  bool over() const override { return driver_.over(); }
  uint64_t digest() const override { return driver_.digest(); }

  std::string observe() override {
    const GameState& s = driver_.state();
    const mg::Level& level = *s.level;
    const Racer& me = s.racers[0];
    const auto& k = me.skier;
    const Progress& p = me.progress;
    std::string out = "{\"x\":" + fixed(k.x) + ",\"y\":" + fixed(k.y) + ",\"z\":" + fixed(k.z) +
                      ",\"heading\":" + fixed(k.heading) + ",\"speed\":" + fixed(k.speed) +
                      ",\"speed_kmh\":" + fixed(k.speed * 3.6) + ",\"edge\":" + fixed(k.edge) +
                      ",\"on_snow\":" + flag(k.on_snow) + ",\"air_time\":" + fixed(k.air_time) +
                      ",\"down\":" + flag(k.down);
    const auto gates = static_cast<int32_t>(level.checkpoints.size());
    out += ",\"gate\":" + std::to_string(p.next) + ",\"gates\":" + std::to_string(gates);
    if (!p.finished && p.next < gates) {
      const mg::Checkpoint& g = level.checkpoints[static_cast<size_t>(p.next)];
      const double bearing = bearing_to_next(level, p, k.x, k.z);
      out += ",\"gate_bearing\":" + fixed(core::angle_diff(k.heading, bearing)) +
             ",\"gate_distance\":" + fixed(core::hypot(g.x - k.x, g.z - k.z));
    }
    const mg::TrackHit hit = mg::nearest_track_point(level.track, k.x, k.z);
    const mg::TrackPoint here = mg::track_point_at(level.track, hit.s);
    const mg::TrackPoint ahead = mg::track_point_at(level.track, hit.s + kBendAhead);
    out += ",\"piste_lateral\":" + fixed(hit.lateral) + ",\"piste_width\":" + fixed(here.width) +
           ",\"piste_heading\":" + fixed(core::angle_diff(k.heading, here.heading)) +
           ",\"piste_bend\":" + fixed(core::angle_diff(here.heading, ahead.heading)) +
           ",\"piste_s\":" + fixed(hit.s) + ",\"piste_length\":" + fixed(level.track.length);
    out += ",\"finished\":" + flag(p.finished);
    if (p.finished) out += ",\"finish_time\":" + fixed(p.finish_time);
    out += ",\"place\":" + std::to_string(place_of(s, 0)) +
           ",\"racers\":" + std::to_string(s.racers.size()) + ",\"events\":[";
    for (size_t i = 0; i < backlog_.size(); i++) {
      const GameEvent& e = backlog_[i];
      out += std::string(i ? "," : "") + "{\"kind\":" + quote(event_name(e.kind)) +
             ",\"racer\":" + std::to_string(e.racer) + ",\"gate\":" + std::to_string(e.gate) +
             ",\"value\":" + fixed(e.value) + "}";
    }
    out += "]";
    if (dropped_ > 0) out += ",\"events_dropped\":" + std::to_string(dropped_);
    backlog_.clear();
    dropped_ = 0;
    return out + "}";
  }

  std::string state() const override {
    const GameState& s = driver_.state();
    std::string out =
        "{\"steps\":" + std::to_string(s.steps) + ",\"t\":" + exact(s.t) + ",\"racers\":[";
    for (size_t i = 0; i < s.racers.size(); i++) {
      const Racer& r = s.racers[i];
      const auto& k = r.skier;
      const Progress& p = r.progress;
      out += std::string(i ? "," : "") + "{\"x\":" + exact(k.x) + ",\"y\":" + exact(k.y) +
             ",\"z\":" + exact(k.z) + ",\"vx\":" + exact(k.vx) + ",\"vy\":" + exact(k.vy) +
             ",\"vz\":" + exact(k.vz) + ",\"heading\":" + exact(k.heading) +
             ",\"yaw_rate\":" + exact(k.yaw_rate) + ",\"edge\":" + exact(k.edge) +
             ",\"tuck\":" + exact(k.tuck) + ",\"lean\":" + exact(k.lean) +
             ",\"sink\":" + exact(k.sink) + ",\"packed\":" + exact(k.packed) +
             ",\"speed\":" + exact(k.speed) + ",\"g_lat\":" + exact(k.g_lat) +
             ",\"jump_held\":" + exact(k.jump_held) + ",\"air_time\":" + exact(k.air_time) +
             ",\"air_peak\":" + exact(k.air_peak) + ",\"down_time\":" + exact(k.down_time) +
             ",\"on_snow\":" + flag(k.on_snow) + ",\"down\":" + flag(k.down) +
             ",\"cause\":" + std::to_string(static_cast<int>(k.cause)) +
             ",\"gate\":" + std::to_string(p.next) + ",\"missed\":" + flag(p.missed) +
             ",\"finished\":" + flag(p.finished) + ",\"finish_time\":" + exact(p.finish_time) +
             ",\"resets\":" + std::to_string(p.resets) +
             ",\"wipeouts\":" + std::to_string(p.wipeouts) +
             ",\"missed_gates\":" + std::to_string(p.missed_gates) +
             ",\"stuck\":" + exact(p.stuck) + "}";
    }
    return out + "]}";
  }

  std::string report() const override {
    const RunReport r = driver_.report(seed_);
    return "{\"finished\":" + flag(r.finished) + ",\"time\":" + fixed(r.time) +
           ",\"place\":" + std::to_string(r.place) +
           ",\"racers\":" + std::to_string(r.field.size()) +
           ",\"gates\":" + std::to_string(r.gates) +
           ",\"gate_count\":" + std::to_string(r.gate_count) +
           ",\"missed\":" + std::to_string(r.missed) + ",\"resets\":" + std::to_string(r.resets) +
           ",\"wipeouts\":" + std::to_string(r.wipeouts) +
           ",\"flights\":" + std::to_string(r.flights) + ",\"top_speed\":" + fixed(r.top_speed) +
           ",\"mean_speed\":" + fixed(r.mean_speed) +
           ",\"max_landing_g\":" + fixed(r.max_landing_g) + "}";
  }

  bool pilot(std::span<double> out) override {
    const Controls c = bot_controls(driver_.state(), 0, driver_.plan());
    out[0] = c.ski.steer;
    out[1] = c.ski.tuck;
    out[2] = c.ski.brake;
    out[3] = c.ski.jump ? 1 : 0;
    out[4] = c.reset ? 1 : 0;
    return true;
  }

 private:
  void collect() {
    for (const GameEvent& e : driver_.state().events) {
      const bool mine = e.racer == 0 || (e.kind == EventKind::shoulder && e.gate == 0);
      if (!mine) continue;
      if (backlog_.size() == kMostEvents) {
        backlog_.erase(backlog_.begin());
        dropped_++;
      }
      backlog_.push_back(e);
    }
  }

  Driver driver_;
  double seed_;
  std::vector<GameEvent> backlog_;
  int32_t dropped_ = 0;
};

std::unique_ptr<ge::play::Run> open_run(const ge::play::Setup& setup, std::string& error) {
  const double rivals = setup.number("rivals");
  const double version = setup.number("version");
  if (rivals < 0 || rivals > 7 || rivals != num::round(rivals)) {
    error = "rivals must be a whole number from 0 to 7";
    return nullptr;
  }
  try {
    // The last map built, kept: `ge diverge` and a replay open the same map twice.
    static double last_seed = 0;
    static double last_version = -1;
    static std::shared_ptr<const mg::Level> last;
    if (!last || last_seed != setup.seed || last_version != version) {
      last = std::make_shared<const mg::Level>(
          race_map(setup.seed, static_cast<int>(num::round(version))));
      last_seed = setup.seed;
      last_version = version;
    }
    std::shared_ptr<const mg::Level> level = last;
    return std::make_unique<FallLineRun>(std::move(level), setup.seed,
                                         static_cast<int32_t>(rivals));
  } catch (const mg::GenerateError& e) {
    error = "seed " + exact(setup.seed) + " built no map: " + e.what() +
            "; pick another seed, or run `make lab LAB=analyze ARGS=--seeds=" + exact(setup.seed) +
            " --refusals`";
    return nullptr;
  }
}

constexpr std::array<ge::play::Axis, 5> kAxes{{
    {"steer", ge::play::AxisKind::signed_unit, "-1 full left .. 1 full right"},
    {"tuck", ge::play::AxisKind::lever, "0 upright .. 1 folded into the tuck: less drag"},
    {"brake", ge::play::AxisKind::lever, "0 .. 1 the skid that scrubs speed"},
    {"jump", ge::play::AxisKind::toggle, "held on the snow loads the jump; release to pop"},
    {"reset", ge::play::AxisKind::press, "stand back on the piste past the last gate taken"},
}};

constexpr std::array<ge::play::Option, 2> kOptions{{
    {"rivals", "0", "bots on the grid beside the player (0..7)"},
    {"version", "1", "the generator's version the race map is built by"},
}};

}  // namespace

const ge::play::GameSpec kGame{
    "fall_line", "a downhill race through the gates of a generated mountain, the field beside you",
    kHz,         600,
    kAxes,       kOptions,
    open_run,
};

}  // namespace fall_line::sim
