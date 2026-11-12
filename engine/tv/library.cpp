// A TV node's finished files (library.hpp): the folder scanned, each file hashed and its header
// read.
//
// ge:summary A TV node's finished files: the folder scanned, hashed, headers read
// ge:keywords tv library scan sha256 header
// ge:skill replay
// ge:test tests/tv_web_test.cpp
#include "tv/library.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "mod/sha256.hpp"

namespace ge::tv {

bool library_scan(const std::string& dir, std::vector<LibraryFile>& out, std::string& error) {
  namespace fs = std::filesystem;
  out.clear();
  std::error_code ec;
  fs::directory_iterator it(dir, ec);
  if (ec) {
    error = "cannot read the folder " + dir + ": " + ec.message();
    return false;
  }
  for (const auto& e : it) {
    if (!e.is_regular_file(ec) || e.path().extension() != ".ger") continue;
    std::ifstream f(e.path(), std::ios::binary);
    std::vector<uint8_t> head(size_t{64} * 1024);
    f.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
    head.resize(static_cast<size_t>(f.gcount()));
    replay::StreamReader r;
    std::string why;
    if (!replay::read_stream_header(head, r, why)) continue;
    const auto hash = mod::sha256_file(e.path());
    if (!hash) continue;
    LibraryFile lf;
    lf.sha256 = *hash;
    lf.name = e.path().filename().string();
    lf.path = e.path().string();
    lf.bytes = static_cast<uint64_t>(e.file_size(ec));
    lf.game = r.header.game;
    lf.meta = r.header.meta;
    out.push_back(std::move(lf));
  }
  std::sort(out.begin(), out.end(),
            [](const LibraryFile& a, const LibraryFile& b) { return a.name < b.name; });
  return true;
}

}  // namespace ge::tv
