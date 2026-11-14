// engine/save/cache.hpp: content kept under a key is read back; a miss, another key behind the same
// name, a damaged entry and a newer chunk version are refused (and the bad entry removed); a disk
// that dies leaves no entry and the content still good.
//
// ge:summary Test: the content cache's hit, miss, refusals and a full disk
// ge:keywords test content-cache cache key hit miss stale damaged
// ge:skill save testing
#include "save/cache.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "save/disk.hpp"

namespace {

using namespace ge::save;

constexpr uint32_t kTag = tag("DATA");

std::vector<double> content() {
  std::vector<double> v(5000);
  for (size_t i = 0; i < v.size(); i++) v[i] = static_cast<double>(i) * 0.25;
  return v;
}

}  // namespace

TEST(content_kept_is_read_back_and_a_miss_is_a_miss) {
  SimDisk disk;
  ContentCache cache(disk, "demo");
  std::vector<double> got;
  CHECK(!cache.get("map seed=38", kTag, 1, got));
  CHECK(cache.counts().misses == 1);
  CHECK_MSG(cache.put("map seed=38", kTag, 1, content()), cache.problem());
  CHECK(cache.get("map seed=38", kTag, 1, got) && got == content());
  CHECK(!cache.get("map seed=7", kTag, 1, got));
  CHECK(cache.counts().hits == 1 && cache.counts().misses == 2 && cache.counts().written == 1);
  CHECK(disk.files.count(ContentCache::file_name("map seed=38")) == 1);
}

TEST(a_damaged_stale_or_foreign_entry_is_refused_and_removed) {
  SimDisk disk;
  ContentCache cache(disk, "demo");
  std::vector<double> got;
  const std::string name = ContentCache::file_name("k");
  // Damaged.
  CHECK(cache.put("k", kTag, 1, content()));
  disk.files[name][disk.files[name].size() / 2] ^= 0x40;
  CHECK(!cache.get("k", kTag, 1, got) && cache.counts().refused == 1);
  CHECK(disk.files.count(name) == 0 && !cache.problem().empty());
  // A newer chunk than this build reads.
  CHECK(cache.put("k", kTag, 2, content()));
  CHECK(!cache.get("k", kTag, 1, got) && cache.counts().refused == 2);
  // Another key's entry under this key's name.
  CHECK(cache.put("other", kTag, 1, content()));
  disk.files[name] = disk.files[ContentCache::file_name("other")];
  CHECK(!cache.get("k", kTag, 1, got) && cache.counts().refused == 3);
  // Another game's.
  ContentCache theirs(disk, "theirs");
  CHECK(theirs.put("k", kTag, 1, content()));
  CHECK(!cache.get("k", kTag, 1, got));
}

TEST(a_disk_that_dies_keeps_no_entry) {
  SimDisk disk;
  disk.budget = 100;
  ContentCache cache(disk, "demo");
  CHECK(!cache.put("k", kTag, 1, content()));
  CHECK(cache.problem().find("next:") != std::string::npos);
  SimDisk back = disk.restarted();
  ContentCache again(back, "demo");
  std::vector<double> got;
  CHECK(!again.get("k", kTag, 1, got));
}
