// A CONTENT PACK'S FILES BY HASH (docs/hub.md § Playing in a browser): beside each content pack it
// makes (engine/hub/content.hpp) the hub keeps every file in it on its own, zstd-packed and named
// by the SHA-256 of its plain bytes (`/content/f/<sha>.zst`), and a list of them
// (`/content/<pack sha>.files`). A page fetches the list and then only the files its browser does
// not hold already, so a second map, or the same map tomorrow, costs what changed. A map's list is
// also asked for by the map's name (`/content/map/<map>.files`, made the first time), which is how
// a joined game that changes map finds the next one's files.
//
// The list is text, a line each: `ge-files 1`, then `pack <sha256>` (the whole pack's, which a
// page that holds little of the list fetches instead, since one pack packs smaller than its files
// one by one), then `<sha256> <bytes> <name>` a file (the name to the end of the line, '/'
// separated). The same words on every platform, so it is written by the hub and read by the page,
// and tested natively.
//
// ge:summary A content pack's files by hash: the list the hub writes and the page reads, the URLs
// ge:keywords web hub content files file-list by-hash cache browser-cache sha256 map-change
// ge:keywords write_file_list read_file_list pack_list_url map_list_url file_url
// ge:skill net
// ge:test tests/web_file_list_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::web {

struct ListedFile {
  std::string sha;  // the plain bytes' SHA-256, 64 lower-case hex digits
  uint64_t bytes = 0;
  std::string name;
};

inline constexpr std::string_view kFileListHead = "ge-files 1";
inline constexpr uint64_t kMostListedBytes = 256ull << 20;  // a file bigger is refused

struct FileList {
  std::string pack;  // the whole pack's SHA-256 ("": none named)
  std::vector<ListedFile> files;
};

std::string write_file_list(const FileList& list);
// False with the line and what is wrong with it.
bool read_file_list(std::string_view text, FileList& out, std::string& error);

// A content pack's URL by its hash, and the list beside a content pack's URL
// (`.../content/<sha>.pak.zst` -> `.../content/<sha>.files`);
// "" when the URL is not a content pack's.
std::string pack_url(std::string_view sha);
std::string pack_list_url(std::string_view pack_url);
// A map's list by its name, and one file by its hash, as the hub serves them.
std::string map_list_url(std::string_view map);
std::string file_url(std::string_view sha);

}  // namespace ge::web
