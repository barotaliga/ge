// The director's definitions (director.hpp): the candidates scored, taken best first under the
// spacing, the length and the count, then put in run order; the reel's clock over the shots.
//
// ge:summary The director's definitions: rules_problem, plan_cut, reel_seconds, reel_at
// ge:keywords replay director plan_cut reel_at reel_seconds rules_problem candidate spacing
// ge:skill replay
// ge:test tests/replay_director_test.cpp
#include "replay/director.hpp"

#include <algorithm>

#include "core/num.hpp"

namespace ge::replay {

namespace {

int64_t steps_of(double seconds, double hz) {
  return static_cast<int64_t>(core::num::round(seconds * hz));
}

const ShotRule* rule_for(std::span<const ShotRule> rules, const std::string& kind) {
  for (const ShotRule& r : rules) {
    if (r.kind == kind) return &r;
  }
  return nullptr;
}

}  // namespace

std::string rules_problem(std::span<const ShotRule> rules) {
  for (size_t i = 0; i < rules.size(); i++) {
    const ShotRule& r = rules[i];
    if (!mark_kind_ok(r.kind)) return "the rule `" + r.kind + "`: a kind is a-z, 0-9 and _";
    if (!(r.rate > 0)) return "the rule `" + r.kind + "`: its rate must be above 0";
    if (!(r.lead_s >= 0) || !(r.hold_s >= 0)) {
      return "the rule `" + r.kind + "`: its lead and hold must be 0 or more";
    }
    for (size_t j = 0; j < i; j++) {
      if (rules[j].kind == r.kind) return "the kind `" + r.kind + "` has two rules";
    }
  }
  return "";
}

Cut plan_cut(std::span<const Mark> marks, std::span<const ShotRule> rules,
             const DirectorPolicy& policy, double hz, int64_t length) {
  Cut cut;
  std::vector<Shot> candidates;
  for (size_t i = 0; i < marks.size(); i++) {
    const Mark& m = marks[i];
    const ShotRule* r = rule_for(rules, m.kind);
    if (!r) {
      cut.unruled.push_back(i);
      continue;
    }
    Shot s;
    s.from = std::clamp<int64_t>(m.step - steps_of(r->lead_s, hz), 0, length);
    s.to = std::clamp<int64_t>(m.step + steps_of(r->hold_s, hz), s.from, length);
    s.mark = i;
    s.kind = m.kind;
    s.lens = r->lens;
    s.rate = r->rate;
    s.score = m.weight * r->scale;
    candidates.push_back(std::move(s));
  }
  std::stable_sort(candidates.begin(), candidates.end(), [&](const Shot& a, const Shot& b) {
    if (a.score != b.score) return a.score > b.score;
    const int64_t sa = marks[a.mark].step, sb = marks[b.mark].step;
    if (sa != sb) return sa < sb;
    return a.mark < b.mark;
  });
  const int64_t spacing = steps_of(policy.spacing_s, hz);
  const int64_t most = policy.most_s > 0 ? steps_of(policy.most_s, hz) : -1;
  int64_t shown = 0;
  for (Shot& c : candidates) {
    const int64_t at = marks[c.mark].step;
    bool inside = false, near = false;
    for (const Shot& s : cut.shots) {
      if (at >= s.from && at <= s.to) inside = true;
      if (c.from < s.to + spacing && s.from < c.to + spacing) near = true;
    }
    if (inside) {
      cut.covered.push_back(c.mark);
      continue;
    }
    const bool too_long = most >= 0 && shown + (c.to - c.from) > most;
    const bool too_many =
        policy.most_shots > 0 && cut.shots.size() >= static_cast<size_t>(policy.most_shots);
    if (near || too_long || too_many) {
      cut.dropped.push_back(c.mark);
      continue;
    }
    shown += c.to - c.from;
    cut.shots.push_back(std::move(c));
  }
  std::sort(cut.shots.begin(), cut.shots.end(),
            [](const Shot& a, const Shot& b) { return a.from < b.from; });
  std::sort(cut.covered.begin(), cut.covered.end());
  std::sort(cut.dropped.begin(), cut.dropped.end());
  return cut;
}

double reel_seconds(const Cut& cut, double hz) {
  double total = 0;
  for (const Shot& s : cut.shots) total += static_cast<double>(s.to - s.from) / hz / s.rate;
  return total;
}

ReelAt reel_at(const Cut& cut, double hz, double seconds) {
  ReelAt at;
  if (cut.shots.empty()) {
    at.over = true;
    return at;
  }
  double left = seconds > 0 ? seconds : 0;
  for (size_t i = 0; i < cut.shots.size(); i++) {
    const Shot& s = cut.shots[i];
    const double span = static_cast<double>(s.to - s.from) / hz / s.rate;
    if (left < span) {
      at.shot = i;
      at.step = static_cast<double>(s.from) + left * s.rate * hz;
      return at;
    }
    left -= span;
  }
  at.shot = cut.shots.size() - 1;
  at.step = static_cast<double>(cut.shots.back().to);
  at.over = true;
  return at;
}

}  // namespace ge::replay
