// A TV NODE'S FINISHED FILES (docs/tv.md § The TV node): the replay files in a folder, each listed
// by the SHA-256 of its bytes (which the hub's archive and its links name a file by), with its
// name, size, game and the meta its header holds. A viewer asks the node for the list and for a
// file by its hash (node.hpp's `list` and `get`), and the node sends the file's bytes.
//
// ge:summary A TV node's finished replay files listed by SHA-256, with their header's meta
// ge:keywords tv library files archive sha256 hash list get ged finished folder library_scan
// ge:skill replay
// ge:test tests/tv_web_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "replay/stream.hpp"

namespace ge::tv {

struct LibraryFile {
  std::string sha256;  // lower-case hex of the whole file
  std::string name;    // the file's name in the folder
  std::string path;
  uint64_t bytes = 0;
  std::string game;
  replay::Meta meta;  // the header's
};

// Every `.ged` file directly in `dir` that opens as a replay file, by name. False with `error`
// when the folder cannot be read.
bool library_scan(const std::string& dir, std::vector<LibraryFile>& out, std::string& error);

}  // namespace ge::tv
