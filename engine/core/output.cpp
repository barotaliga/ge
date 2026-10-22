// The output channel's state; see output.hpp. The recent lines are a ring of 200 strings
// whose storage is reused, so a steady stream of lines stops allocating once the ring has
// filled. The channel table is a fixed array of names and verbosities: `enabled` is one read.
//
// ge:summary The output channel's module state: the sink, the channel table, the recent-line ring
// ge:keywords output log sink ring-buffer module-state channel verbosity configure log-spec
// ge:skill determinism agent-interface debug
// ge:test tests/core_output_test.cpp tests/core_output_channels_test.cpp
#include "core/output.hpp"

#include <array>

namespace ge::core::output {
namespace {

constexpr size_t kRecentCap = 200;

struct ChannelRow {
  std::string name;
  Verbosity verbosity = Verbosity::info;
  bool named = false;  // set by name in the last configure; `*` and the debug switch skip it
};

struct State {
  std::array<LogLine, kRecentCap> ring;
  size_t head = 0;   // index of the oldest line
  size_t count = 0;  // lines held, <= kRecentCap
  Sink sink = nullptr;
  void* user = nullptr;
  std::array<ChannelRow, kMaxChannels> channels;
  int channel_count = 0;
  Verbosity rest = Verbosity::info;  // every channel `configure` did not name
  int64_t step = -1;
  bool overflow_warned = false;
};

State& state() {
  static State s = [] {
    State init;
    init.channels[0].name = "engine";
    init.channel_count = 1;
    return init;
  }();
  return s;
}

// The verbosity a level needs to be kept.
Verbosity needs(Level level) {
  switch (level) {
    case Level::error:
      return Verbosity::error;
    case Level::warn:
      return Verbosity::warn;
    case Level::status:
    case Level::info:
      return Verbosity::info;
    case Level::debug:
      return Verbosity::debug;
    case Level::trace:
      return Verbosity::trace;
  }
  return Verbosity::info;
}

bool parse_verbosity(std::string_view text, Verbosity& out) {
  for (const Verbosity v : {Verbosity::off, Verbosity::error, Verbosity::warn, Verbosity::info,
                            Verbosity::debug, Verbosity::trace}) {
    if (text == verbosity_name(v)) {
      out = v;
      return true;
    }
  }
  return false;
}

bool valid_name(std::string_view name) {
  if (name.empty() || name.size() > 32) return false;
  for (const char c : name) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

int find_channel(const State& s, std::string_view name) {
  for (int i = 0; i < s.channel_count; i++) {
    if (s.channels[static_cast<size_t>(i)].name == name) return i;
  }
  return -1;
}

void emit(Channel ch, Level level, std::string_view message) {
  State& s = state();
  if (needs(level) > s.channels[ch].verbosity) return;
  LogLine* slot;
  if (s.count < kRecentCap) {
    slot = &s.ring[(s.head + s.count) % kRecentCap];
    s.count++;
  } else {
    slot = &s.ring[s.head];  // the oldest line is overwritten
    s.head = (s.head + 1) % kRecentCap;
  }
  slot->level = level;
  slot->channel = ch;
  slot->step = s.step;
  slot->message.assign(message);
  if (s.sink) s.sink(*slot, s.user);
}

std::string_view trim(std::string_view t) {
  while (!t.empty() && t.front() == ' ') t.remove_prefix(1);
  while (!t.empty() && t.back() == ' ') t.remove_suffix(1);
  return t;
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
    case Level::trace:
      return "trace";
  }
  return "status";
}

std::string_view verbosity_name(Verbosity verbosity) {
  switch (verbosity) {
    case Verbosity::off:
      return "off";
    case Verbosity::error:
      return "error";
    case Verbosity::warn:
      return "warn";
    case Verbosity::info:
      return "info";
    case Verbosity::debug:
      return "debug";
    case Verbosity::trace:
      return "trace";
  }
  return "info";
}

void set_output_sink(Sink sink, void* user) {
  State& s = state();
  s.sink = sink;
  s.user = user;
  if (!sink) return;
  for (size_t i = 0; i < s.count; i++) sink(s.ring[(s.head + i) % kRecentCap], user);
}

void set_debug_enabled(bool enabled) {
  State& s = state();
  s.rest = enabled ? Verbosity::debug : Verbosity::info;
  for (int i = 0; i < s.channel_count; i++) {
    ChannelRow& row = s.channels[static_cast<size_t>(i)];
    if (!row.named) row.verbosity = s.rest;
  }
}

bool debug_enabled() { return state().channels[kEngine].verbosity >= Verbosity::debug; }

Channel channel(std::string_view name) {
  State& s = state();
  const int found = find_channel(s, name);
  if (found >= 0) return static_cast<Channel>(found);
  if (!valid_name(name) || s.channel_count >= kMaxChannels) {
    if (!s.overflow_warned) {
      s.overflow_warned = true;
      std::string why = "log channel \"";
      why.append(name);
      why.append(
          "\" logs on `engine` instead: a name is lower case, digits, _ and - (32 at most), and "
          "a process has at most 64 channels. next: rename the channel (docs/debugging.md).");
      emit(kEngine, Level::warn, why);
    }
    return kEngine;
  }
  ChannelRow& row = s.channels[static_cast<size_t>(s.channel_count)];
  row.name.assign(name);
  row.verbosity = s.rest;
  row.named = false;
  return static_cast<Channel>(s.channel_count++);
}

std::string_view channel_name(Channel ch) {
  const State& s = state();
  return ch < s.channel_count ? std::string_view(s.channels[ch].name) : std::string_view("engine");
}

std::vector<std::string_view> channel_names() {
  const State& s = state();
  std::vector<std::string_view> out;
  out.reserve(static_cast<size_t>(s.channel_count));
  for (int i = 0; i < s.channel_count; i++)
    out.emplace_back(s.channels[static_cast<size_t>(i)].name);
  return out;
}

bool enabled(Channel ch, Level level) { return needs(level) <= state().channels[ch].verbosity; }

Verbosity verbosity(Channel ch) { return state().channels[ch].verbosity; }

std::string configure(std::string_view spec) {
  // Parse it whole first, so a bad spec changes nothing.
  std::vector<std::pair<std::string_view, Verbosity>> pairs;
  Verbosity rest = Verbosity::info;
  std::string_view left = spec;
  while (!trim(left).empty()) {
    const size_t comma = left.find(',');
    const std::string_view item = trim(left.substr(0, comma));
    left = comma == std::string_view::npos ? std::string_view() : left.substr(comma + 1);
    if (item.empty()) continue;
    const size_t eq = item.find('=');
    const std::string_view name = eq == std::string_view::npos ? "*" : trim(item.substr(0, eq));
    const std::string_view level = eq == std::string_view::npos ? item : trim(item.substr(eq + 1));
    Verbosity v;
    if (!parse_verbosity(level, v)) {
      std::string why = "log spec item \"";
      why.append(item);
      why.append(
          "\" has no verbosity: write name=off|error|warn|info|debug|trace, comma-separated, * "
          "for the rest (\"skier=trace,*=warn\"); see docs/debugging.md");
      return why;
    }
    if (name == "*") {
      rest = v;
      continue;
    }
    if (!valid_name(name)) {
      std::string why = "log spec channel \"";
      why.append(name);
      why.append("\" is not a channel name: lower case, digits, _ and -; see docs/debugging.md");
      return why;
    }
    pairs.emplace_back(name, v);
  }
  State& s = state();
  s.rest = rest;
  for (int i = 0; i < s.channel_count; i++) {
    ChannelRow& row = s.channels[static_cast<size_t>(i)];
    row.verbosity = rest;
    row.named = false;
  }
  for (const auto& [name, v] : pairs) {
    const Channel ch = channel(name);
    s.channels[ch].verbosity = v;
    s.channels[ch].named = true;
  }
  return "";
}

void set_step(int64_t step) { state().step = step; }

int64_t current_step() { return state().step; }

std::string format_line(const LogLine& line) {
  std::string out;
  const std::string_view ch = channel_name(line.channel);
  out.reserve(line.message.size() + ch.size() + 24);
  out.push_back('[');
  if (line.step < 0)
    out.push_back('-');
  else
    out.append(std::to_string(line.step));
  out.append("] ");
  out.append(level_name(line.level));
  out.push_back(' ');
  out.append(ch);
  out.append(": ");
  out.append(line.message);
  return out;
}

std::vector<LogLine> recent_logs() {
  const State& s = state();
  std::vector<LogLine> out;
  out.reserve(s.count);
  for (size_t i = 0; i < s.count; i++) out.push_back(s.ring[(s.head + i) % kRecentCap]);
  return out;
}

void log_on(Channel ch, Level level, std::string_view message) {
  emit(ch < state().channel_count ? ch : kEngine, level, message);
}

void status(std::string_view message) { emit(kEngine, Level::status, message); }
void info(std::string_view message) { emit(kEngine, Level::info, message); }
void warn(std::string_view message) { emit(kEngine, Level::warn, message); }
void error(std::string_view message) { emit(kEngine, Level::error, message); }
void debug(std::string_view message) { emit(kEngine, Level::debug, message); }
void trace(std::string_view message) { emit(kEngine, Level::trace, message); }

void header(std::string_view message) {
  std::string line;
  line.reserve(message.size() + 16);
  line.append("\xe2\x94\x80\xe2\x94\x80 ");  // "── "
  line.append(message);
  line.append(" \xe2\x94\x80\xe2\x94\x80");  // " ──"
  emit(kEngine, Level::status, line);
}

}  // namespace ge::core::output
