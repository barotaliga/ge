// The play session and the batch runs (session.hpp).
//
// ge:summary Session: the snapped, recorded step, the protocol's ops; pilot_run, replay, diverge
// ge:keywords play session protocol step replay diverge state_at pilot_run
// ge:skill agent-interface
// ge:test tests/play_session_test.cpp
#include "play/session.hpp"

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "play/json_read.hpp"

namespace ge::play {

namespace {

std::string error_line(std::string_view what, std::string_view next) {
  return "{\"op\":\"error\",\"error\":" + quote(what) + ",\"next\":" + quote(next) + "}";
}

// `object` (a JSON object's text) with `fields` spliced in first.
std::string with_fields(const std::string& fields, const std::string& object) {
  const size_t brace = object.find('{');
  if (brace == std::string::npos) return "{" + fields + "}";
  const std::string rest = object.substr(brace + 1);
  const bool empty = rest.find_first_not_of(" \t\r\n") == std::string::npos ||
                     rest[rest.find_first_not_of(" \t\r\n")] == '}';
  return "{" + fields + (empty ? "" : ",") + rest;
}

std::string axis_kind_name(AxisKind k) {
  switch (k) {
    case AxisKind::signed_unit:
      return "signed";
    case AxisKind::lever:
      return "lever";
    case AxisKind::toggle:
      return "toggle";
    case AxisKind::press:
      return "press";
  }
  return "?";
}

std::optional<Played> played_of(const Session& s) {
  Played p;
  p.log = s.inputs();
  p.report = s.run().report();
  p.digest = s.run().digest();
  p.steps = s.steps();
  p.over = s.run().over();
  return p;
}

}  // namespace

std::string quote(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    if (c == '\n') {
      out += "\\n";
    } else if (static_cast<unsigned char>(c) >= 0x20) {
      out += c;
    }
  }
  return out + "\"";
}

Session::Session(const GameSpec& spec, const Setup& setup)
    : spec_(spec), setup_(setup), recorder_(new_log(spec, setup)), snapped_(spec.axes.size()) {
  run_ = spec.open(setup, error_);
  if (!run_ && error_.empty()) error_ = "the game did not open a run";
  most_steps_ = static_cast<int64_t>(core::num::round(spec.most_seconds * spec.hz));
}

void Session::step(std::span<const double> input) {
  for (size_t i = 0; i < snapped_.size(); i++) {
    snapped_[i] = snap_axis(i < input.size() ? input[i] : 0, spec_.axes[i].kind);
  }
  recorder_.record(snapped_);
  run_->step(snapped_);
  steps_++;
}

bool Session::done() const { return run_->over() || steps_ >= most_steps_; }

InputLog Session::inputs() const {
  InputLog log = recorder_.log();
  log.digest = run_->digest();
  return log;
}

std::string Session::hello() const {
  using core::num::to_string;
  std::string axes;
  for (const Axis& a : spec_.axes) {
    axes += std::string(axes.empty() ? "" : ",") + "{\"name\":" + quote(a.name) +
            ",\"kind\":" + quote(axis_kind_name(a.kind)) + ",\"help\":" + quote(a.help) + "}";
  }
  std::string options;
  for (const auto& [name, value] : setup_.options) {
    options += std::string(options.empty() ? "" : ",") + quote(name) + ":" + quote(value);
  }
  return "{\"op\":\"hello\",\"game\":" + quote(spec_.name) + ",\"seed\":" + to_string(setup_.seed) +
         ",\"options\":{" + options + "},\"hz\":" + to_string(spec_.hz) +
         ",\"most_steps\":" + std::to_string(most_steps_) + ",\"axes\":[" + axes +
         "],\"most_hold\":" + std::to_string(kMostHold) +
         ",\"ops\":[\"observe\",\"act\",\"pilot\",\"why\",\"state\",\"inputs\",\"end\"]}";
}

std::string Session::observation(std::string_view op) {
  using core::num::to_fixed;
  return with_fields("\"op\":" + quote(op) + ",\"step\":" + std::to_string(steps_) +
                         ",\"t\":" + to_fixed(static_cast<double>(steps_) / spec_.hz, 3) +
                         ",\"done\":" + (done() ? "true" : "false"),
                     run_->observe());
}

std::string Session::why_now() {
  ai::Why why(0);
  if (!run_->trace(&why)) {
    return error_line(std::string(spec_.name) + "'s bots record no decisions",
                      "trace the game's Run (play/game.hpp), or send pilot");
  }
  std::vector<double> input(spec_.axes.size(), 0);
  const bool has = run_->pilot(input);
  run_->trace(nullptr);
  if (!has) {
    return error_line("the game " + std::string(spec_.name) + " has no pilot",
                      "send act with your own input");
  }
  std::string axes, decisions;
  for (size_t i = 0; i < input.size(); i++) {
    axes += std::string(axes.empty() ? "" : ",") + quote(spec_.axes[i].name) + ":" +
            core::num::to_string(snap_axis(input[i], spec_.axes[i].kind));
  }
  for (const ai::Decision& d : why.decisions()) {
    decisions += std::string(decisions.empty() ? "" : ",") + ai::decision_json(why, d);
  }
  return "{\"op\":\"why\",\"step\":" + std::to_string(steps_) + ",\"pilot\":{" + axes +
         "},\"decisions\":[" + decisions + "]}";
}

std::string Session::handle(std::string_view line) {
  if (ended_) return error_line("the session has ended", "start a new one");
  FlatJson m;
  std::string why;
  if (!read_flat(line, m, why)) {
    return error_line("unreadable line: " + why,
                      "send one JSON object a line, e.g. {\"op\":\"observe\"}");
  }
  const auto op_it = m.find("op");
  if (op_it == m.end() || op_it->second.kind != JsonValue::Kind::string) {
    return error_line("no \"op\" string", "send {\"op\":\"observe\"}; hello lists the ops");
  }
  const std::string& op = op_it->second.text;
  if (op == "observe") return observation(op);
  if (op == "state")
    return with_fields("\"op\":\"state\",\"step\":" + std::to_string(steps_), run_->state());
  if (op == "inputs") {
    return "{\"op\":\"inputs\",\"steps\":" + std::to_string(steps_) +
           ",\"text\":" + quote(to_text(inputs())) + "}";
  }
  if (op == "end") {
    ended_ = true;
    return with_fields("\"op\":\"end\",\"steps\":" + std::to_string(steps_) +
                           ",\"digest\":" + quote(hex64(run_->digest())),
                       run_->report());
  }
  if (op == "why") return why_now();
  if (op != "act" && op != "pilot") {
    return error_line("unknown op \"" + op + "\"",
                      "send observe, act, pilot, why, state, inputs or end");
  }
  if (done())
    return error_line("the run is over; there is nothing to act on", "send {\"op\":\"end\"}");
  double hold = 1;
  if (const auto it = m.find("steps"); it != m.end()) {
    if (it->second.kind != JsonValue::Kind::number) {
      return error_line("\"steps\" is not a number",
                        "send \"steps\" from 1 to " + std::to_string(kMostHold));
    }
    hold = it->second.number;
  }
  if (hold != hold || hold < 1 || hold > kMostHold) {
    return error_line("steps out of range",
                      "send \"steps\" from 1 to " + std::to_string(kMostHold));
  }
  const auto n = static_cast<int32_t>(core::num::round(hold));
  std::vector<double> input(spec_.axes.size(), 0);
  if (op == "pilot") {
    for (int32_t i = 0; i < n && !done(); i++) {
      std::fill(input.begin(), input.end(), 0);
      if (!run_->pilot(input)) {
        return error_line("the game " + std::string(spec_.name) + " has no pilot",
                          "send act with your own input");
      }
      step(input);
    }
    return observation(op);
  }
  for (const auto& [key, v] : m) {
    if (key == "op" || key == "steps") continue;
    bool known = false;
    for (size_t i = 0; i < spec_.axes.size(); i++) {
      if (key != "input." + std::string(spec_.axes[i].name)) continue;
      known = true;
      if (v.kind == JsonValue::Kind::number) {
        input[i] = v.number;
      } else if (v.kind == JsonValue::Kind::boolean) {
        input[i] = v.boolean ? 1 : 0;
      } else if (v.kind != JsonValue::Kind::null) {
        return error_line("input." + std::string(spec_.axes[i].name) + " is not a number",
                          "send a number (true and false for a toggle)");
      }
    }
    if (!known) {
      std::string names;
      for (const Axis& a : spec_.axes) names += std::string(names.empty() ? "" : ", ") + a.name;
      return error_line("unknown key \"" + key + "\"",
                        "send \"input\":{...} with the axes " + names + ", and \"steps\"");
    }
  }
  for (int32_t i = 0; i < n && !done(); i++) {
    step(input);
    for (size_t k = 0; k < input.size(); k++) {
      if (spec_.axes[k].kind == AxisKind::press) input[k] = 0;
    }
  }
  return observation(op);
}

std::optional<Played> pilot_run(const GameSpec& spec, const Setup& setup, std::string& error) {
  Session s(spec, setup);
  if (!s.ok()) {
    error = s.error();
    return std::nullopt;
  }
  std::vector<double> input(spec.axes.size(), 0);
  while (!s.done()) {
    std::fill(input.begin(), input.end(), 0);
    if (!s.run().pilot(input)) {
      error = "the game " + std::string(spec.name) + " has no pilot (its Run::pilot returns false)";
      return std::nullopt;
    }
    s.step(input);
  }
  return played_of(s);
}

bool setup_of(const GameSpec& spec, const InputLog& log, Setup& out, std::string& error) {
  if (!fits(log, spec, error)) return false;
  std::map<std::string, std::string> given(log.options.begin(), log.options.end());
  return make_setup(spec, log.seed, given, out, error);
}

std::optional<Played> replay(const GameSpec& spec, const InputLog& log, std::string& error) {
  Setup setup;
  if (!setup_of(spec, log, setup, error)) return std::nullopt;
  Session s(spec, setup);
  if (!s.ok()) {
    error = s.error();
    return std::nullopt;
  }
  for (int64_t i = 0; i < log.steps(); i++) s.step(log.at(i));
  return played_of(s);
}

std::optional<Still> shot_at(const GameSpec& spec, const InputLog& log, int64_t step,
                             std::string_view view, std::string& error) {
  if (spec.views.empty()) {
    error = std::string(spec.name) + " lists no views";
    return std::nullopt;
  }
  const View* chosen = view.empty() ? &spec.views[0] : nullptr;
  std::string names;
  for (const View& v : spec.views) {
    if (v.name == view) chosen = &v;
    names += std::string(names.empty() ? "" : ", ") + v.name;
  }
  if (!chosen) {
    error = "no view `" + std::string(view) + "`; the views are " + names;
    return std::nullopt;
  }
  if (step < 0 || step > log.steps()) {
    error =
        "step " + std::to_string(step) + " is outside the log's 0.." + std::to_string(log.steps());
    return std::nullopt;
  }
  Setup setup;
  if (!setup_of(spec, log, setup, error)) return std::nullopt;
  Session s(spec, setup);
  if (!s.ok()) {
    error = s.error();
    return std::nullopt;
  }
  for (int64_t i = 0; i < step; i++) s.step(log.at(i));
  Still out;
  out.view = chosen->name;
  out.step = step;
  out.digest = s.run().digest();
  lab::Picture raw;
  if (!s.run().shoot(chosen->name, raw, out.caption, error)) return std::nullopt;
  lab::Shot words;
  words.title = std::string(spec.name) + " " + chosen->name + " at step " + std::to_string(step);
  words.caption = out.caption;
  words.inputs.emplace_back("seed", core::num::to_string(setup.seed));
  for (const auto& [k, v] : setup.options) words.inputs.emplace_back(k, v);
  words.inputs.emplace_back("t", core::num::to_fixed(static_cast<double>(step) / spec.hz, 2) + "s");
  words.inputs.emplace_back("digest", hex64(out.digest));
  out.picture = lab::framed(words, raw);
  return out;
}

std::optional<std::string> state_at(const GameSpec& spec, const InputLog& log, int64_t step,
                                    std::string& error) {
  if (step < 0 || step > log.steps()) {
    error =
        "step " + std::to_string(step) + " is outside the log's 0.." + std::to_string(log.steps());
    return std::nullopt;
  }
  Setup setup;
  if (!setup_of(spec, log, setup, error)) return std::nullopt;
  Session s(spec, setup);
  if (!s.ok()) {
    error = s.error();
    return std::nullopt;
  }
  for (int64_t i = 0; i < step; i++) s.step(log.at(i));
  return s.run().state();
}

namespace {

void compare_states(const std::string& a, const std::string& b, Divergence& d) {
  FlatJson fa, fb;
  std::string why;
  read_flat(a, fa, why, true);
  read_flat(b, fb, why, true);
  auto note = [&](const std::string& key, const std::string& va, const std::string& vb) {
    if (d.fields.size() < kMostFields) {
      d.fields.push_back({key, va, vb});
    } else {
      d.more_fields++;
    }
  };
  for (const auto& [key, va] : fa) {
    const auto it = fb.find(key);
    if (it == fb.end()) {
      note(key, json_text(va), "absent");
    } else if (json_text(va) != json_text(it->second)) {
      note(key, json_text(va), json_text(it->second));
    }
  }
  for (const auto& [key, vb] : fb) {
    if (!fa.contains(key)) note(key, "absent", json_text(vb));
  }
}

}  // namespace

std::optional<Divergence> diverge(const GameSpec& spec, const InputLog& a, const InputLog& b,
                                  std::string& error) {
  Setup sa, sb;
  if (!setup_of(spec, a, sa, error) || !setup_of(spec, b, sb, error)) return std::nullopt;
  Session ra(spec, sa), rb(spec, sb);
  if (!ra.ok() || !rb.ok()) {
    error = ra.ok() ? rb.error() : ra.error();
    return std::nullopt;
  }
  Divergence d;
  d.steps_a = a.steps();
  d.steps_b = b.steps();
  d.setup_differs = sa.seed != sb.seed || sa.options != sb.options;
  const int64_t common = d.steps_a < d.steps_b ? d.steps_a : d.steps_b;
  for (int64_t i = 0; i < common && d.input_step < 0; i++) {
    const auto va = a.at(i);
    const auto vb = b.at(i);
    for (size_t k = 0; k < va.size() && k < vb.size(); k++) {
      if (snap_axis(va[k], spec.axes[k].kind) != snap_axis(vb[k], spec.axes[k].kind)) {
        d.input_step = i;
        break;
      }
    }
  }
  if (d.input_step < 0 && d.steps_a != d.steps_b) d.input_step = common;
  if (ra.run().state() != rb.run().state()) d.state_step = 0;
  for (int64_t i = 0; i < common && d.state_step < 0; i++) {
    ra.step(a.at(i));
    rb.step(b.at(i));
    if (ra.run().digest() != rb.run().digest()) d.state_step = i + 1;
  }
  if (d.state_step >= 0) compare_states(ra.run().state(), rb.run().state(), d);
  // Each to its own end, for the digests.
  while (ra.steps() < d.steps_a) ra.step(a.at(ra.steps()));
  while (rb.steps() < d.steps_b) rb.step(b.at(rb.steps()));
  d.digest_a = ra.run().digest();
  d.digest_b = rb.run().digest();
  d.same = d.state_step < 0 && d.steps_a == d.steps_b && !d.setup_differs;
  return d;
}

}  // namespace ge::play
