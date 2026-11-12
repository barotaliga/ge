// THE DIAGNOSTICS CONVENTION AS A CHECK (docs/architecture.md § Diagnostics). Every error and
// warning is a prompt for an agent: what failed, where, and the next action. This file states
// what "reads as a prompt" means in a form a program can hold, and finds the messages to hold
// to it in C++ and CMake source:
//
//   C++    output::error / output::warn (or through an `out` alias), GE_ASSERT, a lab's fail
//   CMake  message(FATAL_ERROR | SEND_ERROR | WARNING | AUTHOR_WARNING ...)
//
// Only the string literals of a call are judged (the run-time values cannot be), and a call
// with none is skipped. The engine's tests/cli_diagnostics_test.cpp holds the engine, its
// plugins, its games and the scaffold to it; `ge doctor` holds a game repo to the same rule.
//
// ge:summary The diagnostics convention as a check: what reads as a prompt, and where messages are
// ge:keywords diagnostics prompt actionable message error warn fatal-error next-action
// ge:keywords prompt_problem diagnostic_sites lexer agent-output
// ge:skill agent-interface
// ge:test tests/cli_diagnostics_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::cli {

// The words that mark a message's next action, matched without regard to case. One of them
// must appear: "next: <what to do>", "skill: <name>", a command to type ("make test",
// "run tests/...", "rerun"), or a doc to read ("see ...", "docs/...").
inline constexpr std::string_view kNextMarkers[] = {"next:", "skill:", "make ", "run ",
                                                    "rerun", "see ",   "docs/"};

// Why `message` does not read as a prompt, or "" when it does: it says what failed in three
// words or more, and it carries a next-action marker.
std::string prompt_problem(std::string_view message);

enum class SourceKind : uint8_t { cpp, cmake };

struct DiagnosticSite {
  int line;          // 1-based, where the call starts
  std::string call;  // "output::error", "GE_ASSERT", "message(FATAL_ERROR"
  std::string text;  // the call's string literals, joined in order
};

// Every diagnostic call in `source` that carries at least one string literal. Comments and the
// insides of string literals are skipped, so a call quoted in a comment is not a site.
std::vector<DiagnosticSite> diagnostic_sites(std::string_view source, SourceKind kind);

// PLAYER TEXT (docs/architecture.md § Diagnostics, Player text). What a person reads while they
// play (a console line, a chat or centre print, a menu's hint, a refusal from the server) reads
// like a game's own words, never like a prompt for an agent: no "next:", no "skill:", no
// `backtick` markup. The prompt shape above is for the log channels, labs, `ge` and a game's
// player CLI. `player_text_sites` gives every string literal of a C++ source outside the log and
// diagnostic calls (output::*, out::*, GE_ASSERT, a lab's fail, static_assert); a file whose
// leading comment carries kAgentVoiceMarker (a lab's harness, a tool's main) is not player text.
inline constexpr std::string_view kAgentVoiceMarker = "voice: agent";

// Why `literal` does not read as a game's words, or "" when it does.
std::string player_text_problem(std::string_view literal);

struct TextSite {
  int line;          // 1-based
  std::string text;  // one string literal's contents, escapes resolved
};
std::vector<TextSite> player_text_sites(std::string_view source);

// Whether `path` (relative to a game's or the engine's root, '/'-separated) holds player text:
// any file under games/<game>/ outside its labs, cli, play, assets, content, mods and qc, and
// engine/console, engine/server and engine/hud.
bool is_player_text_path(std::string_view path);

}  // namespace ge::cli
