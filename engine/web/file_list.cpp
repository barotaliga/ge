// A content pack's files by hash (file_list.hpp): the list written and read, the URLs.
//
// ge:summary A content pack's files by hash defined: the list's text both ways, the URLs
// ge:keywords web file-list read_file_list write_file_list pack_list_url
// ge:skill net
// ge:test tests/web_file_list_test.cpp
#include "web/file_list.hpp"

#include <algorithm>

namespace ge::web {

namespace {

bool hex64(std::string_view s) {
  return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

}  // namespace

std::string write_file_list(const FileList& list) {
  std::string out(kFileListHead);
  out += '\n';
  if (!list.pack.empty()) out += "pack " + list.pack + "\n";
  for (const ListedFile& f : list.files)
    out += f.sha + " " + std::to_string(f.bytes) + " " + f.name + "\n";
  return out;
}

bool read_file_list(std::string_view text, FileList& out, std::string& error) {
  out = {};
  size_t line_no = 0;
  bool head = false;
  while (!text.empty()) {
    const size_t nl = text.find('\n');
    std::string_view line = text.substr(0, nl);
    text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
    line_no++;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty()) continue;
    const std::string at = "file list line " + std::to_string(line_no);
    if (!head) {
      if (line != kFileListHead) {
        error = at + ": '" + std::string(line.substr(0, 40)) + "' is not '" +
                std::string(kFileListHead) + "'; the hub may be newer than this page";
        return false;
      }
      head = true;
      continue;
    }
    if (line.substr(0, 5) == "pack ") {
      if (!hex64(line.substr(5))) {
        error = at + ": the pack's hash is not 64 hex digits";
        return false;
      }
      out.pack = std::string(line.substr(5));
      continue;
    }
    const size_t a = line.find(' ');
    const size_t b = a == std::string_view::npos ? a : line.find(' ', a + 1);
    if (b == std::string_view::npos || !hex64(line.substr(0, a))) {
      error = at + ": not '<sha256> <bytes> <name>'";
      return false;
    }
    ListedFile f;
    f.sha = std::string(line.substr(0, a));
    const std::string_view size = line.substr(a + 1, b - a - 1);
    if (size.empty() || size.size() > 12 ||
        !std::all_of(size.begin(), size.end(), [](char c) { return c >= '0' && c <= '9'; })) {
      error = at + ": the size '" + std::string(size) + "' is not a number";
      return false;
    }
    for (char c : size) f.bytes = f.bytes * 10 + static_cast<uint64_t>(c - '0');
    f.name = std::string(line.substr(b + 1));
    if (f.bytes > kMostListedBytes || f.name.empty()) {
      error = at + ": " + (f.name.empty() ? "no name" : "a file past 256 MB");
      return false;
    }
    out.files.push_back(std::move(f));
  }
  if (!head) {
    error = "the file list is empty";
    return false;
  }
  return true;
}

std::string pack_list_url(std::string_view pack_url) {
  constexpr std::string_view kDir = "content/", kExt = ".pak.zst";
  if (pack_url.size() < kDir.size() + 64 + kExt.size() ||
      pack_url.substr(pack_url.size() - kExt.size()) != kExt)
    return "";
  const size_t sha_at = pack_url.size() - kExt.size() - 64;
  if (!hex64(pack_url.substr(sha_at, 64)) ||
      pack_url.substr(sha_at - kDir.size(), kDir.size()) != kDir)
    return "";
  return std::string(pack_url.substr(0, sha_at + 64)) + ".files";
}

std::string pack_url(std::string_view sha) { return "/content/" + std::string(sha) + ".pak.zst"; }

std::string map_list_url(std::string_view map) {
  return "/content/map/" + std::string(map) + ".files";
}

std::string file_url(std::string_view sha) { return "/content/f/" + std::string(sha) + ".zst"; }

}  // namespace ge::web
