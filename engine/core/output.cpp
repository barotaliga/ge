// The output channel's state; see output.hpp. The recent lines are a ring of 200 strings
// whose storage is reused, so a steady stream of lines stops allocating once the ring has
// filled.
//
// ge:summary The output channel's module state: the sink, the debug switch, the recent-line ring
// ge:keywords output log sink ring-buffer module-state
// ge:skill determinism agent-interface
// ge:test tests/core_output_test.cpp
#include "core/output.hpp"

#include <array>

namespace ge::core::output {
namespace {

constexpr size_t kRecentCap = 200;

struct State {
  std::array<LogLine, kRecentCap> ring;
  size_t head = 0;   // index of the oldest line
  size_t count = 0;  // lines held, <= kRecentCap
  Sink sink = nullptr;
  void* user = nullptr;
  bool debug = false;
};

State& state() {
  static State s;
  return s;
}

void emit(Level level, std::string_view message) {
  State& s = state();
  if (level == Level::debug && !s.debug) return;
  LogLine* slot;
  if (s.count < kRecentCap) {
    slot = &s.ring[(s.head + s.count) % kRecentCap];
    s.count++;
  } else {
    slot = &s.ring[s.head];  // the oldest line is overwritten
    s.head = (s.head + 1) % kRecentCap;
  }
  slot->level = level;
  slot->message.assign(message);
  if (s.sink) s.sink(level, message, s.user);
}

}  // namespace

std::string_view level_name(Level level) {
  switch (level) {
    case Level::status:
      return "status";
    case Level::info:
      return "info";
    case Level::warn:
      return "warn";
    case Level::error:
      return "error";
    case Level::debug:
      return "debug";
  }
  return "status";
}

void set_output_sink(Sink sink, void* user) {
  State& s = state();
  s.sink = sink;
  s.user = user;
  if (!sink) return;
  for (size_t i = 0; i < s.count; i++) {
    const LogLine& line = s.ring[(s.head + i) % kRecentCap];
    sink(line.level, line.message, user);
  }
}

void set_debug_enabled(bool enabled) { state().debug = enabled; }

bool debug_enabled() { return state().debug; }

std::vector<LogLine> recent_logs() {
  const State& s = state();
  std::vector<LogLine> out;
  out.reserve(s.count);
  for (size_t i = 0; i < s.count; i++) out.push_back(s.ring[(s.head + i) % kRecentCap]);
  return out;
}

void status(std::string_view message) { emit(Level::status, message); }
void info(std::string_view message) { emit(Level::info, message); }
void warn(std::string_view message) { emit(Level::warn, message); }
void error(std::string_view message) { emit(Level::error, message); }
void debug(std::string_view message) { emit(Level::debug, message); }

void header(std::string_view message) {
  std::string line;
  line.reserve(message.size() + 16);
  line.append("\xe2\x94\x80\xe2\x94\x80 ");  // "── "
  line.append(message);
  line.append(" \xe2\x94\x80\xe2\x94\x80");  // " ──"
  emit(Level::status, line);
}

}  // namespace ge::core::output
