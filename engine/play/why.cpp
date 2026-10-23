// A log replayed with the decision trace on, its timeline and its flips (why.hpp).
//
// ge:summary why_replay, why_timeline and why_flips over a Session and an ai::Why
// ge:keywords why replay timeline flips series pilot agrees
// ge:skill bots agent-interface
// ge:test tests/play_why_test.cpp
#include "play/why.hpp"

#include <cmath>
#include <limits>
#include <string>

#include "play/session.hpp"

namespace ge::play {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

double choice_index(const ai::Why& why, const ai::Decision& d) {
  double i = 0;
  for (const ai::Choice& c : why.options(d)) {
    if (std::string_view(c.name) == d.chosen) return i;
    i++;
  }
  return kNaN;
}

}  // namespace

std::optional<WhyRun> why_replay(const GameSpec& spec, const InputLog& log, int32_t agent,
                                 std::string& error) {
  Setup setup;
  if (!setup_of(spec, log, setup, error)) return std::nullopt;
  Session s(spec, setup);
  if (!s.ok()) {
    error = s.error();
    return std::nullopt;
  }
  WhyRun out;
  out.why = ai::Why(agent);
  if (!s.run().trace(&out.why)) {
    error = std::string(spec.name) + "'s bots record no decisions (its Run has no trace)";
    return std::nullopt;
  }
  std::vector<double> wanted(spec.axes.size(), 0);
  out.asked_pilot = true;
  out.agrees.reserve(static_cast<size_t>(log.steps()));
  for (int64_t i = 0; i < log.steps(); i++) {
    const std::span<const double> given = log.at(i);
    if (out.asked_pilot && s.run().pilot(wanted)) {
      bool same = true;
      for (size_t k = 0; k < wanted.size(); k++) {
        same = same && snap_axis(wanted[k], spec.axes[k].kind) == given[k];
      }
      out.agrees.push_back(same ? 1 : 0);
    } else {
      out.asked_pilot = false;
      out.agrees.clear();
    }
    s.step(given);
  }
  s.run().trace(nullptr);
  out.steps = s.steps();
  out.digest = s.run().digest();
  return out;
}

std::vector<const ai::Decision*> decisions_between(const ai::Why& why, int64_t from, int64_t to,
                                                   std::string_view name) {
  std::vector<const ai::Decision*> out;
  for (const ai::Decision& d : why.decisions()) {
    if (d.step < from || d.step > to) continue;
    if (!name.empty() && name != d.name) continue;
    out.push_back(&d);
  }
  return out;
}

lab::Series why_timeline(const ai::Why& why, int32_t agent, double hz) {
  // The columns: every decision's in the order first met, each fact in the order first met.
  struct Named {
    std::string name;
    std::vector<std::string> facts;
  };
  std::vector<Named> names;
  std::vector<int64_t> steps;
  for (const ai::Decision& d : why.decisions()) {
    if (d.agent != agent) continue;
    if (steps.empty() || steps.back() != d.step) steps.push_back(d.step);
    Named* n = nullptr;
    for (Named& m : names) n = m.name == d.name ? &m : n;
    if (!n) n = &names.emplace_back(Named{d.name, {}});
    for (const ai::Fact& f : why.facts(d)) {
      bool have = false;
      for (const std::string& x : n->facts) have = have || x == f.name;
      if (!have) n->facts.emplace_back(f.name);
    }
  }
  lab::Series s;
  s.set("title", "agent " + std::to_string(agent) + "'s decisions");
  s.set("x", "t_s");
  s.set("unit.t_s", "s");
  auto add = [&](std::string name, bool text) -> lab::Column& {
    lab::Column c;
    c.name = std::move(name);
    c.text = text;
    if (text) {
      c.cell.assign(steps.size(), "");
    } else {
      c.num.assign(steps.size(), kNaN);
    }
    s.columns.push_back(std::move(c));
    return s.columns.back();
  };
  add("step", false);
  add("t_s", false);
  for (size_t r = 0; r < steps.size(); r++) {
    s.columns[0].num[r] = static_cast<double>(steps[r]);
    s.columns[1].num[r] = hz > 0 ? static_cast<double>(steps[r]) / hz : kNaN;
  }
  for (const Named& n : names) {
    add(n.name, true);
    add(n.name + ".choice", false);
    add(n.name + ".margin", false);
    for (const std::string& f : n.facts) add(n.name + "." + f, false);
  }
  size_t row = 0;
  for (const ai::Decision& d : why.decisions()) {
    if (d.agent != agent) continue;
    while (steps[row] != d.step) row++;
    lab::Column* c = s.find(d.name);
    c->cell[row] = d.chosen;
    s.find(std::string(d.name) + ".choice")->num[row] = choice_index(why, d);
    s.find(std::string(d.name) + ".margin")->num[row] = d.margin;
    for (const ai::Fact& f : why.facts(d)) {
      s.find(std::string(d.name) + "." + f.name)->num[row] = f.value;
    }
  }
  return s;
}

std::vector<Flip> why_flips(const ai::Why& why, int32_t agent, double close) {
  std::vector<Flip> out;
  // The last choice of each decision, by name.
  std::vector<std::pair<std::string_view, const char*>> last;
  for (const ai::Decision& d : why.decisions()) {
    if (d.agent != agent) continue;
    const char* before = nullptr;
    for (auto& [name, chosen] : last) {
      if (name == d.name) {
        before = chosen;
        chosen = d.chosen;
      }
    }
    if (!before) last.emplace_back(d.name, d.chosen);
    const bool changed = before && std::string_view(before) != d.chosen;
    const bool tight = close > 0 && !std::isnan(d.margin) && d.margin < close;
    if (changed || tight) out.push_back({d.step, d.name, before ? before : "", d.chosen, d.margin});
  }
  return out;
}

}  // namespace ge::play
