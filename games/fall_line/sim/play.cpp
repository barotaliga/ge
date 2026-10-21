// The agent's session (play.hpp): the line reader, the observation, the ops.
//
// ge:summary PlaySession: the JSON line reader, the observation and the four ops
// ge:keywords play json reader observation act tape end hello
// ge:skill sim agent-interface
// ge:test tests/fall_line_play_test.cpp
#include "fall_line/sim/play.hpp"

#include <map>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/controls.hpp"
#include "fall_line/sim/course.hpp"
#include "fall_line/sim/field.hpp"
#include "fall_line/sim/game.hpp"
#include "mountain/mapgen/query.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace core = ge::core;
namespace num = ge::core::num;

namespace {

// A JSON value as the protocol needs it: a number, a bool, a string or null.
struct Value {
  enum class Kind : uint8_t { number, boolean, string, null } kind = Kind::null;
  double number = 0;
  bool boolean = false;
  std::string text;
};

// A JSON object read flat: every scalar under its dotted path ("input.steer"). Arrays are not
// part of the protocol and are refused.
class LineReader {
 public:
  explicit LineReader(std::string_view s) : s_(s) {}

  bool read(std::map<std::string, Value>& out, std::string& error) {
    skip();
    if (!object("", out)) {
      error = error_.empty() ? "expected a JSON object" : error_;
      return false;
    }
    skip();
    if (i_ != s_.size()) {
      error = "text after the object at column " + std::to_string(i_ + 1);
      return false;
    }
    return true;
  }

 private:
  void skip() {
    while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\r')) i_++;
  }
  bool eat(char c) {
    skip();
    if (i_ < s_.size() && s_[i_] == c) {
      i_++;
      return true;
    }
    return false;
  }
  bool fail(const std::string& what) {
    if (error_.empty()) error_ = what + " at column " + std::to_string(i_ + 1);
    return false;
  }
  bool string(std::string& out) {
    if (!eat('"')) return fail("expected a string");
    while (i_ < s_.size() && s_[i_] != '"') {
      if (s_[i_] == '\\') {
        if (++i_ >= s_.size()) break;
        const char e = s_[i_];
        out += e == 'n' ? '\n' : e == 't' ? '\t' : e;
      } else {
        out += s_[i_];
      }
      i_++;
    }
    if (i_ >= s_.size()) return fail("an unterminated string");
    i_++;
    return true;
  }
  bool number(double& out) {
    const size_t from = i_;
    while (i_ < s_.size() && (s_[i_] == '-' || s_[i_] == '+' || s_[i_] == '.' || s_[i_] == 'e' ||
                              s_[i_] == 'E' || (s_[i_] >= '0' && s_[i_] <= '9'))) {
      i_++;
    }
    const std::string text(s_.substr(from, i_ - from));
    size_t used = 0;
    try {
      out = std::stod(text, &used);
    } catch (...) {
      used = 0;
    }
    if (text.empty() || used != text.size()) return fail("a malformed number");
    return true;
  }
  bool value(const std::string& path, std::map<std::string, Value>& out) {
    skip();
    if (i_ >= s_.size()) return fail("expected a value");
    const char c = s_[i_];
    Value v;
    if (c == '{') return object(path + ".", out);
    if (c == '[') return fail("an array (the protocol has none)");
    if (c == '"') {
      v.kind = Value::Kind::string;
      if (!string(v.text)) return false;
    } else if (s_.substr(i_, 4) == "true" || s_.substr(i_, 5) == "false") {
      v.kind = Value::Kind::boolean;
      v.boolean = c == 't';
      i_ += v.boolean ? 4 : 5;
    } else if (s_.substr(i_, 4) == "null") {
      i_ += 4;
    } else {
      v.kind = Value::Kind::number;
      if (!number(v.number)) return false;
    }
    out[path] = std::move(v);
    return true;
  }
  bool object(const std::string& prefix, std::map<std::string, Value>& out) {
    if (!eat('{')) return fail("expected '{'");
    if (eat('}')) return true;
    do {
      skip();
      std::string key;
      if (!string(key)) return false;
      if (!eat(':')) return fail("expected ':'");
      if (!value(prefix + key, out)) return false;
    } while (eat(','));
    if (!eat('}')) return fail("expected ',' or '}'");
    return true;
  }

  std::string_view s_;
  size_t i_ = 0;
  std::string error_;
};

std::string fixed(double v) { return num::to_fixed(v, 3); }
std::string flag(bool b) { return b ? "true" : "false"; }
// A JSON string: quotes and backslashes escaped, control characters dropped.
std::string quoted(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    if (static_cast<unsigned char>(c) >= 0x20) out += c;
  }
  return out + "\"";
}

std::string hex(uint64_t u) {
  static const char* kDigits = "0123456789abcdef";
  std::string s(16, '0');
  for (int i = 15; i >= 0; i--, u >>= 4) s[static_cast<size_t>(i)] = kDigits[u & 15];
  return s;
}

std::string error(std::string_view what, std::string_view next) {
  return "{\"op\":\"error\",\"error\":" + quoted(what) + ",\"next\":" + quoted(next) + "}";
}

// A number the input may carry, 0 when absent; false when present and not a number.
bool number_at(const std::map<std::string, Value>& m, const std::string& key, double& out) {
  const auto it = m.find(key);
  if (it == m.end() || it->second.kind == Value::Kind::null) return true;
  if (it->second.kind != Value::Kind::number) return false;
  out = it->second.number;
  return true;
}
bool bool_at(const std::map<std::string, Value>& m, const std::string& key, bool& out) {
  const auto it = m.find(key);
  if (it == m.end() || it->second.kind == Value::Kind::null) return true;
  if (it->second.kind != Value::Kind::boolean) return false;
  out = it->second.boolean;
  return true;
}

}  // namespace

PlaySession::PlaySession(std::shared_ptr<const mg::Level> level, double seed, int32_t rivals)
    : driver_(std::move(level), rivals), seed_(seed) {
  backlog_.reserve(kMostEvents);
}

std::string PlaySession::hello() const {
  const GameState& s = driver_.state();
  return "{\"op\":\"hello\",\"seed\":" + fixed(seed_) +
         ",\"racers\":" + std::to_string(s.racers.size()) +
         ",\"gates\":" + std::to_string(s.level->checkpoints.size()) +
         ",\"length\":" + fixed(s.level->track.length) + ",\"hz\":" + fixed(kHz) +
         ",\"most_steps\":" + std::to_string(kMostSteps) +
         ",\"ops\":[\"observe\",\"act\",\"tape\",\"end\"]}";
}

void PlaySession::collect() {
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

std::string PlaySession::observation(std::string_view op) {
  const GameState& s = driver_.state();
  const mg::Level& level = *s.level;
  const Racer& me = s.racers[0];
  const auto& k = me.skier;
  const Progress& p = me.progress;
  std::string out = "{\"op\":" + quoted(op) + ",\"t\":" + fixed(s.t) +
                    ",\"step\":" + std::to_string(s.steps) + ",\"x\":" + fixed(k.x) +
                    ",\"y\":" + fixed(k.y) + ",\"z\":" + fixed(k.z) +
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
         ",\"piste_s\":" + fixed(hit.s);
  out += ",\"finished\":" + flag(p.finished);
  if (p.finished) out += ",\"finish_time\":" + fixed(p.finish_time);
  out += ",\"place\":" + std::to_string(place_of(s, 0)) +
         ",\"racers\":" + std::to_string(s.racers.size()) + ",\"events\":[";
  for (size_t i = 0; i < backlog_.size(); i++) {
    const GameEvent& e = backlog_[i];
    out += std::string(i ? "," : "") + "{\"kind\":" + quoted(event_name(e.kind)) +
           ",\"racer\":" + std::to_string(e.racer) + ",\"gate\":" + std::to_string(e.gate) +
           ",\"value\":" + fixed(e.value) + "}";
  }
  out += "]";
  if (dropped_ > 0) out += ",\"events_dropped\":" + std::to_string(dropped_);
  out += "}";
  backlog_.clear();
  dropped_ = 0;
  return out;
}

std::string PlaySession::handle(std::string_view line) {
  if (ended_) return error("the session has ended", "start a new session");
  std::map<std::string, Value> m;
  std::string why;
  if (!LineReader(line).read(m, why)) {
    return error("unreadable line: " + why,
                 "send one JSON object a line, e.g. {\"op\":\"observe\"}");
  }
  const auto op_it = m.find("op");
  if (op_it == m.end() || op_it->second.kind != Value::Kind::string) {
    return error("no \"op\" string", "send {\"op\":\"observe\"}, act, tape or end");
  }
  const std::string& op = op_it->second.text;
  if (op == "observe") return observation(op);
  if (op == "act") {
    if (driver_.over()) {
      return error("every racer is home; there is nothing to act on", "send {\"op\":\"end\"}");
    }
    double steps = 1;
    Controls c;
    bool jump = false;
    bool reset = false;
    if (!number_at(m, "steps", steps) || !number_at(m, "input.steer", c.ski.steer) ||
        !number_at(m, "input.tuck", c.ski.tuck) || !number_at(m, "input.brake", c.ski.brake) ||
        !bool_at(m, "input.jump", jump) || !bool_at(m, "input.reset", reset)) {
      return error("a mistyped input",
                   "steer, tuck, brake and steps are numbers; jump and reset are true or false");
    }
    if (steps != steps || steps < 1 || steps > kMostSteps) {
      return error("steps out of range", "send \"steps\" from 1 to " + std::to_string(kMostSteps));
    }
    c.ski.jump = jump;
    c.reset = reset;
    const auto n = static_cast<int32_t>(num::round(steps));
    for (int32_t i = 0; i < n && !driver_.over(); i++) {
      driver_.step(c);
      c.reset = false;
      collect();
    }
    return observation(op);
  }
  if (op == "tape") {
    const ge::racing::ControlTape tape = driver_.tape();
    std::string out = "{\"op\":\"tape\",\"steps\":" + std::to_string(tape.steps) + ",\"axes\":{";
    for (size_t i = 0; i < kTapeSchema.size() && i < tape.streams.size(); i++) {
      out +=
          std::string(i ? "," : "") + quoted(kTapeSchema[i].name) + ":" + quoted(tape.streams[i]);
    }
    return out + "}}";
  }
  if (op == "end") {
    ended_ = true;
    const RunReport r = driver_.report(seed_);
    return "{\"op\":\"end\",\"finished\":" + flag(r.finished) + ",\"time\":" + fixed(r.time) +
           ",\"place\":" + std::to_string(r.place) + ",\"gates\":" + std::to_string(r.gates) +
           ",\"gate_count\":" + std::to_string(r.gate_count) +
           ",\"missed\":" + std::to_string(r.missed) + ",\"resets\":" + std::to_string(r.resets) +
           ",\"wipeouts\":" + std::to_string(r.wipeouts) +
           ",\"flights\":" + std::to_string(r.flights) + ",\"top_speed\":" + fixed(r.top_speed) +
           ",\"steps\":" + std::to_string(r.steps) + ",\"digest\":" + quoted(hex(r.digest)) + "}";
  }
  return error("unknown op \"" + op + "\"", "send observe, act, tape or end");
}

}  // namespace fall_line::sim
