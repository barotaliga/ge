// HOW AN AGENT LEARNS THE ENGINE FROM THE RELEASE IT BUILDS ON: `ge find` and `ge explain`.
// Every source file in the engine and in a game repo ends its leading comment with a `ge:` tag
// block (summary, keywords, skill, test); this module reads those blocks, from the game repo and
// from the engine's source tree, and answers two questions:
//
//   ge find <word>...   which files, skills and lessons match any word, ranked (a keyword hit
//                       outranks a summary or path hit)
//   ge explain <x>      what one file, function or type is for: the file's header comment and
//                       tags, and for a name its declaration with the comment above it
//
// The engine's tree is the one the game is pinned to: `--engine=<dir>`, else the repo itself when
// it is the engine, else the source CMake fetched for a build (build/*/_deps/gameengine2-src, or
// the FETCHCONTENT_SOURCE_DIR_GAMEENGINE2 a build was configured with), else the tree this ge was
// built from. No network, no index file: the tags are read where they live.
//
// ge:summary ge find and ge explain: the ge: tag blocks of the repo and the pinned engine, searched
// ge:keywords cli ge find explain search discovery ge-tags keywords index engine-root
// ge:keywords tagged-file read_tags find_engine_root tagged_files run_find run_explain declaration
// ge:skill agent-interface
// ge:test tests/cli_index_test.cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cli/outcome.hpp"

namespace ge::cli {

// One source file's tag block and the prose above it.
struct TaggedFile {
  std::string path;    // relative to its root, '/' separators
  std::string origin;  // "repo" or "engine"
  fs::path abs;
  std::string summary;
  std::vector<std::string> keywords;
  std::vector<std::string> skills;
  std::vector<std::string> tests;
  std::string header;  // the leading comment's prose, without the comment markers
};

// The tag block of one file (a C++, CMake or shell file), or nothing when it has none.
std::optional<TaggedFile> read_tags(const fs::path& root, const fs::path& file,
                                    const std::string& origin);

// The engine source tree this repo builds on, by the order in the header; nothing when none is
// found. `flag` is --engine's value ("" when not given).
std::optional<fs::path> find_engine_root(const fs::path& repo, const std::string& flag);

// Every tagged file in the repo and, when it is a different tree, the engine's (engine/,
// plugins/, games/), sorted by origin then path.
std::vector<TaggedFile> tagged_files(const fs::path& repo, const std::optional<fs::path>& engine);

// `ge find`: the outcome's result is {"engine","files":[...],"skills":[...],"lessons":[...]}.
Outcome run_find(const fs::path& repo, const std::string& engine_flag,
                 const std::vector<std::string>& words, int limit);

// `ge explain`: a path (as the repo or the engine spells it, or as an #include does: core/x.hpp),
// a function or type name, or else a word whose best match is explained.
Outcome run_explain(const fs::path& repo, const std::string& engine_flag,
                    const std::string& target);

}  // namespace ge::cli
