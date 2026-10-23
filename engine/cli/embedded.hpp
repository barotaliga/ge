// WHAT ge CARRIES: the consumer skills (consumer-skills/<name>/SKILL.md), the game scaffold
// (scaffold/**) and the engine's version, compiled in by cmake/EmbedFiles.cmake at build time.
// A ge therefore writes the skills and the scaffold of exactly the engine it was built from,
// which is why a game repo runs the ge its engine pin builds (`make ge` there).
//
// ge:summary The files compiled into ge: consumer skills, the scaffold, the engine version
// ge:keywords cli ge embedded embed consumer-skills scaffold version engine_version
// ge:keywords embedded_files
// ge:skill agent-interface
// ge:test tests/cli_skills_test.cpp
#pragma once

#include <span>
#include <string_view>

namespace ge::cli {

struct EmbeddedFile {
  std::string_view path;  // from the engine's root: "consumer-skills/labs/SKILL.md"
  std::string_view text;  // the file's bytes as committed
};

// Every embedded file, sorted by path.
std::span<const EmbeddedFile> embedded_files();

// The engine's VERSION ("0.1.0"): the release this ge belongs to, and the pin `ge new` writes.
std::string_view engine_version();

// Where the engine lives: the release a scaffold fetches, the place a game reports an engine bug.
inline constexpr std::string_view kEngineRepo = "https://github.com/barotaliga/ge";

}  // namespace ge::cli
