// THE ONE OUTPUT CHANNEL. Every diagnostic
// line the engine and a game emit goes through these semantic helpers, never straight to
// stdout or stderr (core does no I/O: printing is the host's sink). The engine runs under
// more than one host (the app, a headless CLI, a test), so the sink is pluggable: the host
// attaches one, and lines emitted before it did are
// kept in a buffer of the last 200 and replayed into it, so boot output is never lost.
// Every line is written for an agent to act on: what failed, where, and the next action
// (docs/architecture.md § Diagnostics).
//
// CHANNELS: a subsystem or a game logs on a named channel of its own (`channel("skier")`) so a
// reader can switch on exactly the detail they are digging for. Every channel keeps lines up
// to its VERBOSITY (`info` unless configured: status, info, warn and error kept, debug and trace
// dropped); `configure("skier=trace,bot=debug,*=warn")` sets them, `*` being every channel not
// named. The plain helpers below write on the `engine` channel. A line carries the simulation
// STEP it was emitted in (`set_step`, -1 outside a run), so a log dump lines up with a trace,
// a replay and `ge state`. Check `enabled(ch, level)` before formatting a line that is likely
// dropped: a dropped line then costs one table read and no allocation (docs/debugging.md).
//
// MODULE STATE: the sink, the channel table, the step and the buffer live in output.cpp, one
// per process, shared by every caller (that is the point: one log). It is single-threaded: emit
// from the simulation's thread only.
// Bits: tests/core_output_test.cpp against tests/golden/core_output.txt; the channels by
// tests/core_output_channels_test.cpp.
//
// ge:summary The one diagnostic channel: log helpers, named channels, a pluggable sink, a buffer
// ge:keywords output log logging diagnostics sink status info warn error debug trace
// ge:keywords header module-state channel verbosity configure log-spec set_step format_line
// ge:keywords debug-logging ge-log
// ge:skill determinism agent-interface debug
// ge:test tests/core_output_test.cpp tests/core_output_channels_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::core::output {

// `trace` is the firehose under `debug`: a line every step of every racer.
enum class Level : uint8_t { status, info, warn, error, debug, trace };

// "status", "info", "warn", "error", "debug" or "trace": the level's name as a line prints it.
std::string_view level_name(Level level);

// How much of a channel is kept: each level keeps itself and everything before it.
enum class Verbosity : uint8_t { off, error, warn, info, debug, trace };

// "off", "error", "warn", "info", "debug" or "trace".
std::string_view verbosity_name(Verbosity verbosity);

// A channel's handle: an index into the channel table. Channel 0 is `engine`.
using Channel = uint8_t;
inline constexpr Channel kEngine = 0;
// At most this many channels a process; a name past it logs on `engine` (with one warning).
inline constexpr int kMaxChannels = 64;

struct LogLine {
  Level level;
  Channel channel;
  int64_t step;  // the simulation step it was emitted in, -1 outside a run
  std::string message;
};

// A host's sink. `user` is handed back untouched (the host's own context).
using Sink = void (*)(const LogLine& line, void* user);

// Attach the host's sink (nullptr detaches); the buffered lines are replayed into it.
void set_output_sink(Sink sink, void* user = nullptr);

// Lift debug-level lines into the sink and the buffer (a `--debug` flag, a dev build): the
// verbosity of every channel not named by `configure` becomes debug (false: back to info).
void set_debug_enabled(bool enabled);
// Whether the `engine` channel keeps debug lines.
bool debug_enabled();

// The channel called `name` (lower case, digits, `_` and `-`), registered on first use. Call it
// once and keep the handle (`static const Channel ch = channel("skier");`).
Channel channel(std::string_view name);
std::string_view channel_name(Channel ch);
// Every channel registered so far, in registration order (`engine` first).
std::vector<std::string_view> channel_names();
// Whether a line of `level` on `ch` would be kept.
bool enabled(Channel ch, Level level);
Verbosity verbosity(Channel ch);

// THE LOG SPEC: comma-separated `name=verbosity` pairs, `*` for every channel not named, and a
// bare verbosity meaning `*=<it>` ("debug", "skier=trace,*=warn"). Names not yet registered are
// registered, so a spec can be read before the code that logs on them runs. Returns "" or what
// was wrong with the spec and how to write it (nothing is applied then). An empty spec resets
// every channel to info.
std::string configure(std::string_view spec);

// The step every line from now on is stamped with (-1: outside a run). A game's step calls it.
void set_step(int64_t step);
int64_t current_step();

// A line as a dump writes it: "[1234] debug skier: message" ("[-]" outside a run).
std::string format_line(const LogLine& line);

// Log `message` on `ch` at `level` (dropped unless `enabled`).
void log_on(Channel ch, Level level, std::string_view message);

// The buffered tail of everything emitted so far, oldest first (at most 200 lines).
std::vector<LogLine> recent_logs();

// A normal progress or state line ("Stage generated").
void status(std::string_view message);
// Supplementary detail a reader only wants when digging.
void info(std::string_view message);
// Something odd but recoverable; the game continues.
void warn(std::string_view message);
// A failure the player or developer should know about.
void error(std::string_view message);
// A section marker, emitted at status level as "── message ──".
void header(std::string_view message);
// Development-only detail; dropped unless set_debug_enabled(true) ran.
void debug(std::string_view message);
// The firehose: dropped unless the engine channel is configured to trace.
void trace(std::string_view message);

}  // namespace ge::core::output
