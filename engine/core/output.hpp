// THE ONE OUTPUT CHANNEL. Every diagnostic
// line the engine and a game emit goes through these semantic helpers, never straight to
// stdout or stderr (core does no I/O: printing is the host's sink). The engine runs under
// more than one host (the app, a headless CLI, a test), so the sink is pluggable: the host
// attaches one, and lines emitted before it did are
// kept in a buffer of the last 200 and replayed into it, so boot output is never lost.
// Every line is written for an agent to act on: what failed, where, and the next action
// (docs/architecture.md § Diagnostics).
//
// MODULE STATE: the sink, the debug switch and the buffer live in output.cpp, one per
// process, shared by every caller (that is the point: one log). It is single-threaded: emit
// from the simulation's thread only.
// Bits: tests/core_output_test.cpp against tests/golden/core_output.txt.
//
// ge:summary The one diagnostic channel: semantic log helpers, a pluggable sink, a boot buffer
// ge:keywords output log logging diagnostics sink status info warn error debug
// ge:keywords header module-state
// ge:skill determinism agent-interface
// ge:test tests/core_output_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::core::output {

enum class Level : uint8_t { status, info, warn, error, debug };

// "status", "info", "warn", "error" or "debug": the level's name as a line prints it.
std::string_view level_name(Level level);

// A host's sink. `user` is handed back untouched (the host's own context).
using Sink = void (*)(Level level, std::string_view message, void* user);

struct LogLine {
  Level level;
  std::string message;
};

// Attach the host's sink (nullptr detaches); the buffered lines are replayed into it.
void set_output_sink(Sink sink, void* user = nullptr);

// Lift debug-level lines into the sink and the buffer (a `--debug` flag, a dev build).
void set_debug_enabled(bool enabled);
// Whether debug lines are kept, so a caller can skip formatting one that would be dropped.
bool debug_enabled();

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

}  // namespace ge::core::output
