// A CONTENT PACK'S FILES FETCHED BY HASH (Emscripten builds only, GE_WEB; docs/hub.md § Playing in
// a browser): the page reads a list (web/file_list.hpp) and takes each file it names that it does
// not hold this session from the browser's own store when the browser kept it before (the Cache
// Storage `ge-content`, which a browser gives a secure page: https, or the machine itself). What
// the store lacks comes from the hub: the whole pack when the store lacks most of it, since one
// pack packs smaller than its files one by one, else each file it lacks by its hash. Every file is
// checked against its hash before it is used or kept, so a damaged or stale copy is never drawn
// from; the files from the hub are kept for the next visit.
//
// The files come back as one pack in memory, which the game mounts over what it has.
//
// ge:summary A pack's files fetched by hash into the page, kept in the browser for next time
// ge:keywords web fetch_listed cache-storage browser-cache by-hash file-list content map-change
// ge:skill net
#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace ge::web {

struct FileStore {
  std::set<std::string> held;  // the hashes fetched this session, never fetched again
  uint64_t from_hub = 0;       // bytes the hub sent (packed)
  uint64_t from_browser = 0;   // bytes the browser had kept
  uint64_t fetched = 0;        // files the browser did not have
};

// The files `list_url` names that `store` does not hold, fetched and checked, as a pack (`pak`,
// empty when every file was held); `files` the number in it. False with the reason. With
// `progress`, the status line counts the files as they come.
bool fetch_listed(const std::string& list_url, FileStore& store, std::vector<uint8_t>& pak,
                  size_t& files, std::string& error, const std::string& progress = "");

}  // namespace ge::web
