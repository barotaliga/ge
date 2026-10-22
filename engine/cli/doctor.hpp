// `ge doctor`: A GAME REPO HELD TO THE ENGINE'S RULES, every finding a prompt. A game repo's
// `make agents-check` is this command, so the rules hold there the way `make agents-check` and
// `make libm-check` hold them in the engine. It checks:
//
//   guidance     AGENTS.md is a file; CLAUDE.md, GEMINI.md, .cursorrules, the Copilot file and
//                .claude/skills, .gemini/skills are symlinks onto it and .agents/skills
//   skills       every skill's SKILL.md, name, description, .lessons/ and route from AGENTS.md;
//                every lesson fragment's format; the engine's skills as sync wrote them
//   engine pin   cmake/engine.cmake pins a release, the same one this ge belongs to, ideally
//                with its checksum
//   build        no fast-math or FP-contraction flag in any CMake file; ge::core is linked
//   sources      the 1000-line cap; every source file's ge: tag block (its skills and tests
//                exist); sim/ and labs/ call no platform transcendental, and sim/ reads no clock
//                and no unseeded random source
//   diagnostics  every error and warning in C++ and CMake reads as a prompt (cli/diagnostic.hpp)
//
// ge:summary ge doctor: a game repo's guidance, skills, engine pin, build flags and sources checked
// ge:keywords cli ge doctor check agents-check guidance symlinks skills engine-pin fast-math
// ge:keywords ffp-contract libm ge-tags line-cap diagnostics determinism check_diagnostics_in
// ge:skill agent-interface
// ge:test tests/cli_doctor_test.cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cli/finding.hpp"

namespace ge::cli {

struct EnginePin {
  std::string version;  // "0.1.0"
  std::string sha256;   // "" when the pin carries no checksum
};
// The pin cmake/engine.cmake states, or nothing when it states none.
std::optional<EnginePin> read_pin(const fs::path& repo);

// Every check above. The repo passes when no finding is an error.
std::vector<Finding> doctor(const fs::path& repo);

// The diagnostics check for one file (C++ or CMake, by its name; a trailing `.in` is ignored):
// one finding per message that does not read as a prompt. The engine's own test runs it over
// the engine's tree.
std::vector<Finding> check_diagnostics_in(const fs::path& repo, const fs::path& file);

}  // namespace ge::cli
