// `ge new`: A GAME REPO FROM NOTHING. It writes the scaffold this ge carries (scaffold/**,
// every `.in` suffix dropped and every placeholder filled), the agent guidance's symlinks, and
// the engine's skills (`ge skills sync`), so the new repo builds against the engine release it
// pins, runs its own tests and labs, and passes its own `make agents-check` (`ge doctor`) before
// a line of the game is written. `--game=racer` starts it as a racer instead of the demo (kGames).
// The placeholders a scaffold file may use:
//
//   {{name}}            the game's name, lower-case kebab ("snow-racer")
//   {{ident}}           the same as a C++ identifier ("snow_racer"): namespaces, targets
//   {{engine_version}}  the release the repo pins: this ge's own version
//   {{engine_sha256}}   the release's -src.tar.gz checksum, or "" (ge doctor notes it)
//   {{engine_repo}}     where the engine lives
//
// ge:summary ge new: a game repo scaffolded against the engine release this ge belongs to
// ge:keywords cli ge new scaffold template game-repo consumer symlinks placeholders new_game
// ge:keywords agents-md fetchcontent engine-pin starting-game racer demo
// ge:skill agent-interface
// ge:test tests/cli_scaffold_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/finding.hpp"

namespace ge::cli {

// The agent guidance's links, in every game repo as in the engine: (link, target as written).
struct Link {
  std::string_view path;
  std::string_view target;
  bool directory;
};
inline constexpr Link kLinks[] = {
    {"CLAUDE.md", "AGENTS.md", false},
    {"GEMINI.md", "AGENTS.md", false},
    {".cursorrules", "AGENTS.md", false},
    {".github/copilot-instructions.md", "../AGENTS.md", false},
    {".claude/skills", "../.agents/skills", true},
    {".gemini/skills", "../.agents/skills", true},
};

// The pin file a scaffold writes and `ge doctor` reads.
inline constexpr std::string_view kPinFile = "cmake/engine.cmake";

// A game's name: lower-case kebab, starting with a letter.
bool valid_game_name(std::string_view name);
// The name as a C++ identifier: '-' becomes '_'.
std::string ident_of(std::string_view name);

struct NewOptions {
  fs::path dir;               // created; must not exist or be empty
  std::string name;           // empty: the directory's name
  std::string engine_sha256;  // the release's -src.tar.gz checksum, when known
  std::string game = "demo";  // which game the scaffold starts as (kGames)
};

// THE STARTING GAMES `ge new --game=<name>` writes. `demo` is the scaffold as it stands (a body
// pushed across a field by the steer and a seeded gust); any other is the scaffold with the files
// under templates/game-<name>/ written over it, path for path (a racer: a car round a loop of
// gates, laps, an autopilot, a track view).
inline constexpr std::string_view kGames[] = {"demo", "racer"};

struct NewResult {
  std::vector<std::string> written;  // repository-relative paths
  std::vector<Finding> findings;     // any error means the repo is incomplete
};

NewResult new_game(const NewOptions& options);

}  // namespace ge::cli
