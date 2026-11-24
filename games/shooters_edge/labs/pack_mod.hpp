// A mission pack's packs laid over the game's, as the game mounts a mod folder: the labs that
// play a pack's maps on its own program (`--mod=hipnotic`, `--mod=rogue`) read the one pack this
// writes. Header-only, beside the labs (a lab's helper never goes into a library: the labs skill).
//
// ge:summary The labs' mission pack: a mod folder's packs laid over the game's, the latest winning
// ge:keywords lab mission-pack mod hipnotic rogue lay_mod pack-layers mod-folder
// ge:skill shooter labs
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "vfs/pak.hpp"

namespace shooters_edge::labs {

// The mod folder's packs (`<game>/<mod>/pak0.pak`, `pak1.pak` ... in order, or `<mod>` beside
// `<game>`) laid over `base`: every file of every pack once, the latest copy of a path winning.
// False with the reason.
inline bool lay_mod(const std::string& game, const std::string& mod, std::vector<uint8_t>& base,
                    std::string& error) {
  namespace fs = std::filesystem;
  fs::path dir = fs::path(game) / mod;
  if (!fs::is_directory(dir)) dir = fs::path(game).parent_path() / mod;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  std::map<std::string, size_t> at;
  const auto take = [&](std::span<const uint8_t> bytes) {
    ge::vfs::Pak pak;
    std::string why;
    if (!ge::vfs::read_pak(bytes, &pak, &why)) return false;
    for (const ge::vfs::PakEntry& e : pak.entries) {
      const std::span<const uint8_t> b = ge::vfs::pak_file(bytes, e);
      const auto [it, fresh] = at.emplace(e.path, files.size());
      if (fresh)
        files.emplace_back(e.path, std::vector<uint8_t>(b.begin(), b.end()));
      else
        files[it->second].second.assign(b.begin(), b.end());
    }
    return true;
  };
  take(base);
  int read = 0;
  for (int n = 0;; n++) {
    const fs::path p = dir / ("pak" + std::to_string(n) + ".pak");
    std::ifstream in(p, std::ios::binary);
    if (!in) break;
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
    if (!take(bytes)) {
      error = p.string() + " does not read as a pack";
      return false;
    }
    read++;
  }
  if (read == 0) {
    error = "no pak0.pak in " + dir.string() + "; next: pass --mod=<a folder beside the game's>";
    return false;
  }
  return ge::vfs::write_pak(files, &base, &error);
}

}  // namespace shooters_edge::labs
