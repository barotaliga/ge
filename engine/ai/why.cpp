// The decision record's storage, its margin and its two printed forms (why.hpp).
//
// ge:summary Why's records, the margin at a choice, the why channel's line, text and JSON forms
// ge:keywords ai why decision record margin decision_text decision_json channel
// ge:skill bots
// ge:test tests/ai_why_test.cpp
#include "ai/why.hpp"

#include <cmath>
#include <limits>

#include "core/num_text.hpp"
#include "core/output.hpp"

namespace ge::ai {

namespace {

namespace out = ge::core::output;
namespace num = ge::core::num;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Three decimals: the text is read, the JSON keeps every bit.
std::string number(double v) { return std::isnan(v) ? "nan" : num::to_fixed(v, 3); }
std::string json_number(double v) { return std::isfinite(v) ? num::to_string(v) : "null"; }
// A name is a literal in the code: only a quote or a backslash needs escaping.
std::string json_name(const char* s) {
  std::string q = "\"";
  for (const char* c = s; *c; c++) {
    if (*c == '"' || *c == '\\') q += '\\';
    q += *c;
  }
  return q + "\"";
}

}  // namespace

Why* Why::follow(int32_t agent, int64_t step) {
  if (agent_ != kEvery && agent != agent_) return nullptr;
  at_agent_ = agent;
  at_step_ = step;
  return this;
}

void Why::decide(const char* name) {
  Decision d;
  d.agent = at_agent_;
  d.step = at_step_;
  d.name = name;
  d.first_fact = static_cast<uint32_t>(facts_.size());
  d.first_option = static_cast<uint32_t>(options_.size());
  decisions_.push_back(d);
  open_ = true;
}

void Why::fact(const char* name, double value) {
  if (!open_) return;
  facts_.push_back({name, value});
  decisions_.back().facts++;
}

void Why::option(const char* name, double score) {
  if (!open_) return;
  Choice c;
  c.name = name;
  c.score = score;
  c.first_term = static_cast<uint32_t>(terms_.size());
  options_.push_back(c);
  decisions_.back().options++;
}

void Why::term(const char* name, double value) {
  if (!open_ || decisions_.back().options == 0) return;
  terms_.push_back({name, value});
  options_.back().terms++;
}

void Why::bar(const char* reason) {
  if (!open_ || decisions_.back().options == 0) return;
  options_.back().barred = reason;
}

void Why::choose(const char* chosen, const char* because) {
  if (!open_) return;
  open_ = false;
  Decision& d = decisions_.back();
  d.chosen = chosen;
  d.because = because;
  double mine = kNaN;
  double best_other = -std::numeric_limits<double>::infinity();
  bool other = false;
  for (const Choice& c : options(d)) {
    if (std::string_view(c.name) == chosen) {
      mine = c.score;
    } else if (!c.barred) {
      best_other = c.score > best_other ? c.score : best_other;
      other = true;
    }
  }
  d.margin = other && !std::isnan(mine) ? mine - best_other : kNaN;
  static const out::Channel kWhy = out::channel("why");
  if (out::enabled(kWhy, out::Level::debug)) {
    out::log_on(kWhy, out::Level::debug,
                "agent " + std::to_string(d.agent) + " " + d.name + ": " + d.chosen +
                    (std::isnan(d.margin) ? "" : " by " + num::to_fixed(d.margin, 3)) + " (" +
                    d.because + ")");
  }
}

std::span<const Fact> Why::facts(const Decision& d) const {
  return std::span<const Fact>(facts_).subspan(d.first_fact, d.facts);
}

std::span<const Choice> Why::options(const Decision& d) const {
  return std::span<const Choice>(options_).subspan(d.first_option, d.options);
}

std::span<const Term> Why::terms(const Choice& c) const {
  return std::span<const Term>(terms_).subspan(c.first_term, c.terms);
}

double Why::fact_of(const Decision& d, const char* name) const {
  for (const Fact& f : facts(d)) {
    if (std::string_view(f.name) == name) return f.value;
  }
  return kNaN;
}

void Why::clear() {
  decisions_.clear();
  facts_.clear();
  options_.clear();
  terms_.clear();
  open_ = false;
}

std::string decision_text(const Why& why, const Decision& d) {
  std::string s = "step " + std::to_string(d.step) + " agent " + std::to_string(d.agent) + " " +
                  d.name + ": " + d.chosen;
  if (!std::isnan(d.margin)) s += " by " + number(d.margin);
  s += std::string(" (") + d.because + ")\n";
  if (d.facts > 0) {
    s += "  saw";
    for (const Fact& f : why.facts(d)) s += std::string(" ") + f.name + "=" + number(f.value);
    s += "\n";
  }
  for (const Choice& c : why.options(d)) {
    s += std::string("  ") + c.name + " " + number(c.score);
    bool first = true;
    for (const Term& t : why.terms(c)) {
      s += std::string(first ? " = " : " + ") + t.name + " " + number(t.value);
      first = false;
    }
    if (c.barred) s += std::string(" (barred: ") + c.barred + ")";
    s += "\n";
  }
  return s;
}

std::string decision_json(const Why& why, const Decision& d) {
  std::string s = "{\"agent\":" + std::to_string(d.agent) + ",\"step\":" + std::to_string(d.step) +
                  ",\"name\":" + json_name(d.name) + ",\"chosen\":" + json_name(d.chosen) +
                  ",\"because\":" + json_name(d.because) + ",\"margin\":" + json_number(d.margin) +
                  ",\"facts\":{";
  bool first = true;
  for (const Fact& f : why.facts(d)) {
    s += std::string(first ? "" : ",") + json_name(f.name) + ":" + json_number(f.value);
    first = false;
  }
  s += "},\"options\":[";
  first = true;
  for (const Choice& c : why.options(d)) {
    s += std::string(first ? "" : ",") + "{\"name\":" + json_name(c.name) +
         ",\"score\":" + json_number(c.score) +
         ",\"barred\":" + (c.barred ? json_name(c.barred) : std::string("null")) + ",\"terms\":{";
    bool ft = true;
    for (const Term& t : why.terms(c)) {
      s += std::string(ft ? "" : ",") + json_name(t.name) + ":" + json_number(t.value);
      ft = false;
    }
    s += "}}";
    first = false;
  }
  return s + "]}";
}

}  // namespace ge::ai
