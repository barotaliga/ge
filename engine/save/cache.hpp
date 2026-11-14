// THE CONTENT CACHE (docs/saves.md § Fast loads: the content cache): what a setup builds slowly (a
// generated map and its indices) written once to a disk under its KEY and read back on the next
// load instead of built again. The key names everything the content depends on: the game's
// words for the setup (the seed, the generator's version) and the build's stamp of the code
// that makes it (`ge_source_stamp` in cmake/SourceStamp.cmake), so a change to the generator's
// code is a new key, never a stale map.
//
// An entry is a save file (file.hpp) of kind `content`: its meta's `setup` holds the whole key
// (a file named by the key's hash is checked against it), one chunk holds the value. A cache is
// a cache: `get` answers false on a miss, a stale or damaged entry, or another key behind the
// same name, and the caller builds the content and `put`s it; a damaged entry is removed. A full
// disk only costs the next load its speed.
//
// ge:summary A content cache on a disk: slow-built content kept by key, rebuilt when stale
// ge:keywords content-cache cache key stamp map generator fast-load miss hit stale damaged
// ge:skill save
// ge:test tests/save_cache_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "save/disk.hpp"
#include "save/file.hpp"
#include "save/io.hpp"

namespace ge::save {

struct CacheCounts {
  int hits = 0;
  int misses = 0;   // no entry under the key
  int refused = 0;  // an entry there that did not read: damaged, another key, a newer version
  int written = 0;
  uint64_t bytes_read = 0;
  uint64_t bytes_written = 0;
};

class ContentCache {
 public:
  // Entries of `game` on `disk` (a directory of their own: the cache lists and removes nothing
  // else).
  ContentCache(Disk& disk, std::string game);

  // The value under `key` in chunk `tag` (at most version `known`) into `v`. False on a miss or a
  // refused entry; `v` is then whatever the reader left and must be built afresh.
  template <class T>
  bool get(std::string_view key, uint32_t tag, uint16_t known, T& v) {
    FileReader r;
    if (!open(key, r)) return false;
    if (!r.get(tag, 0, known, v)) return refuse(key, r.problem());
    counts_.hits++;
    return true;
  }
  // `v` kept under `key` in chunk `tag` at `version`. False (with `problem()`) when the disk
  // refuses; the content is still good, only not kept.
  template <class T>
  bool put(std::string_view key, uint32_t tag, uint16_t version, const T& v) {
    FileWriter w(scratch_, meta_of(key));
    w.put(tag, 0, version, v);
    w.finish();
    return keep(key);
  }

  // The entry's file name for `key`: "content-" and the key's xxh64 in hex.
  static std::string file_name(std::string_view key);

  const CacheCounts& counts() const { return counts_; }
  const std::string& problem() const { return problem_; }

 private:
  Meta meta_of(std::string_view key) const;
  bool open(std::string_view key, FileReader& r);
  bool refuse(std::string_view key, const std::string& why);
  bool keep(std::string_view key);
  Disk* disk_;
  std::string game_;
  std::vector<uint8_t> scratch_;
  CacheCounts counts_;
  std::string problem_;
};

}  // namespace ge::save
