// The humanness of a run off its input log and trace (human.hpp).
//
// ge:summary human_report and human_json: a log's axes and its pilot's decisions, flagged
// ge:keywords humanness report axes decisions json out-of-band
// ge:skill bots agent-interface
// ge:test tests/play_human_test.cpp
#include "play/human.hpp"

#include <cmath>
#include <string_view>

#include "core/num_text.hpp"
#include "play/session.hpp"

namespace ge::play {

namespace {

std::string num(double v) { return std::isfinite(v) ? core::num::to_string(v) : "null"; }

}  // namespace

HumanReport human_report(const GameSpec& spec, const InputLog& log, const ai::Why& why) {
  HumanReport r;
  const double seconds = log.hz > 0 ? static_cast<double>(log.steps()) / log.hz : 0;
  for (size_t k = 0; k < spec.axes.size(); k++) {
    const Axis& axis = spec.axes[k];
    const bool stick = axis.kind == AxisKind::signed_unit;
    if (!stick && axis.kind != AxisKind::lever) continue;
    std::vector<double> pressed;
    pressed.reserve(static_cast<size_t>(log.steps()));
    for (const InputRun& run : log.runs) {
      for (int64_t i = 0; i < run.count; i++) pressed.push_back(run.values[k]);
    }
    std::vector<double> wanted;
    for (const ai::Decision& d : why.decisions()) {
      if (d.agent != 0 || std::string_view(d.name) != "hands") continue;
      wanted.push_back(why.fact_of(d, axis.name));
    }
    if (wanted.size() != pressed.size()) wanted.clear();
    for (const double w : wanted) {
      if (std::isnan(w)) {
        wanted.clear();
        break;
      }
    }
    const ai::AxisHumanness h = ai::axis_humanness(pressed, wanted, stick ? -1 : 0, 1, 0, log.hz);
    r.axes.emplace_back(axis.name, h);
    ai::check_axis(axis.name, stick ? ai::Feel::stick : ai::Feel::trigger, h, r.out);
  }
  std::vector<std::string> names;
  for (const ai::Decision& d : why.decisions()) {
    if (d.agent != 0 || std::string_view(d.name) == "hands") continue;
    bool have = false;
    for (const std::string& n : names) have = have || n == d.name;
    if (!have) names.emplace_back(d.name);
  }
  for (const std::string& n : names) {
    const ai::DecisionHumanness h = ai::decision_humanness(why, 0, n.c_str(), seconds, kCloseCall);
    r.decisions.emplace_back(n, h);
    ai::check_decision(n, h, r.out);
  }
  return r;
}

std::string human_json(const HumanReport& r) {
  std::string s = "{\"axes\":[";
  for (size_t i = 0; i < r.axes.size(); i++) {
    const auto& [name, h] = r.axes[i];
    s += std::string(i ? "," : "") + "{\"name\":" + quote(name) +
         ",\"saturated\":" + num(h.saturated) + ",\"jumps\":" + num(h.jumps) +
         ",\"reversals_min\":" + num(h.reversals_min) + ",\"rate\":" + num(h.rate) +
         ",\"hold_s\":" + num(h.hold_s) + ",\"lag_s\":" + num(h.lag_s) + "}";
  }
  s += "],\"decisions\":[";
  for (size_t i = 0; i < r.decisions.size(); i++) {
    const auto& [name, h] = r.decisions[i];
    s += std::string(i ? "," : "") + "{\"name\":" + quote(name) +
         ",\"flips_min\":" + num(h.flips_min) + ",\"close_share\":" + num(h.close_share) +
         ",\"variety\":" + num(h.variety) + ",\"calls\":" + std::to_string(h.calls) + "}";
  }
  s += "],\"out_of_band\":[";
  for (size_t i = 0; i < r.out.size(); i++) {
    s += std::string(i ? "," : "") + quote(ai::out_of_band_text(r.out[i]));
  }
  return s + "]}";
}

}  // namespace ge::play
