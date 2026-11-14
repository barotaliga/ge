// The content cache (cache.hpp): an entry's name from its key, opened and checked, kept safely.
//
// ge:summary The content cache's entries: named by the key's hash, checked against the key, kept
// ge:keywords content-cache cache key entry refuse keep
// ge:skill save
// ge:test tests/save_cache_test.cpp
#include "save/cache.hpp"

#include <cstdio>

#include "save/xxh64.hpp"

namespace ge::save {

ContentCache::ContentCache(Disk& disk, std::string game) : disk_(&disk), game_(std::move(game)) {}

std::string ContentCache::file_name(std::string_view key) {
  char hex[17];
  std::snprintf(hex, sizeof hex, "%016llx",
                static_cast<unsigned long long>(xxh64(key.data(), key.size())));
  return "content-" + std::string(hex);
}

Meta ContentCache::meta_of(std::string_view key) const {
  Meta m;
  m.game = game_;
  m.kind = Kind::content;
  m.title = "cached content";
  m.setup = std::string(key);
  return m;
}

bool ContentCache::open(std::string_view key, FileReader& r) {
  problem_.clear();
  const std::string name = file_name(key);
  if (!disk_->read(name, scratch_)) {
    counts_.misses++;
    return false;
  }
  counts_.bytes_read += scratch_.size();
  if (!r.open(scratch_, name, game_)) return refuse(key, r.problem());
  if (r.header().kind != Kind::content || r.meta().setup != key)
    return refuse(key, "it holds another key");
  return true;
}

bool ContentCache::refuse(std::string_view key, const std::string& why) {
  counts_.refused++;
  problem_ = "the cached content for " + std::string(key) + " was not used (" + why +
             "); it is built again";
  disk_->remove(file_name(key));
  return false;
}

bool ContentCache::keep(std::string_view key) {
  const std::string name = file_name(key), tmp = name + ".tmp";
  if (!disk_->write(tmp, scratch_) || !disk_->rename(tmp, name)) {
    problem_ = "the content for " + std::string(key) + " was not cached: " + disk_->problem() +
               "; next: free space on the disk (the next load builds it again)";
    disk_->remove(tmp);
    return false;
  }
  counts_.written++;
  counts_.bytes_written += scratch_.size();
  return true;
}

}  // namespace ge::save
