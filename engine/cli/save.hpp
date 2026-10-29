// `ge save`: save files read without the game (docs/saves.md).
//
//   ge save inspect <file>   the header, the meta a menu lists, and every chunk: its tag, its
//                            instance, its version, its offset, its bytes and its checksum;
//                            a damaged or newer file comes back as one finding naming the part
//
//   ge save write <log> --step=N --out=<file>   the log played to step N and saved by the game
//   ge save load <file> [--steps=N]             loaded (map built), then the pilot plays on
//   ge save check <log> [--every=S]             saved every S s, each loaded and played on to
//                                               the log's end beside the run never saved
//   ge save diff <a> <b>                        chunk by chunk, then field by field
//
// `inspect` needs no game and no build: the file format is the engine's (engine/save/file.hpp).
// The others build the game's play executable and run its `save` command (engine/play/saves.hpp).
//
// ge:summary ge save: inspect a save file; write, load, check and diff a game's run saves
// ge:keywords cli ge save inspect file header meta chunk directory checksum damaged
// ge:keywords write load check diff run_save_game
// ge:skill agent-interface save
// ge:test tests/cli_save_test.cpp
#pragma once

#include <string>

#include "cli/commands.hpp"
#include "cli/outcome.hpp"
#include "cli/play.hpp"

namespace ge::cli {

Outcome run_save_inspect(const std::string& file);
// A parsed `ge save <inspect>` line, run.
Outcome run_save(const Args& a);
// `ge save <write|load|check|diff>` (`sub`), run by the game's play executable.
Outcome run_save_game(const BuildOptions& o, const Args& a, const std::string& sub);

}  // namespace ge::cli
