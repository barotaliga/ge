// Each classic game's install kept by game, and its edition (installs.hpp).
//
// ge:summary Keeps each classic game's install and reads its edition off its packs
// ge:keywords installs remember_install remembered_installs edition_of shareware registered
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#include "shooters_edge/client/installs.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_folder.hpp"

namespace shooters_edge::client {

namespace fs = std::filesystem;

namespace {

constexpr const char* kInstallsFile = "installs.txt";

}  // namespace

std::map<std::string, std::string, std::less<>> remembered_installs(const std::string& prefs) {
  std::map<std::string, std::string, std::less<>> out;
  if (prefs.empty()) return out;
  std::ifstream f(fs::path(prefs) / kInstallsFile);
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t space = line.find(' ');
    if (space == std::string::npos || space == 0 || space + 1 >= line.size()) continue;
    out[line.substr(0, space)] = line.substr(space + 1);
  }
  return out;
}

bool remember_install(const std::string& prefs, std::string_view id, const std::string& basedir) {
  if (prefs.empty() || id.empty() || basedir.empty()) return false;
  auto kept = remembered_installs(prefs);
  kept[std::string(id)] = basedir;
  std::error_code ec;
  fs::create_directories(prefs, ec);
  const fs::path file = fs::path(prefs) / kInstallsFile;
  const fs::path tmp = fs::path(prefs) / (std::string(kInstallsFile) + ".tmp");
  {
    std::ofstream f(tmp, std::ios::trunc);
    if (!f) return false;
    for (const auto& [game, folder] : kept) f << game << ' ' << folder << '\n';
    if (!f) return false;
  }
  fs::rename(tmp, file, ec);
  return !ec;
}

Edition edition_of(const std::string& basedir, const ge::classicenhance::Profile& content) {
  SearchPath files;
  std::string error;
  if (!open_game_dir(basedir, std::string(content.base_dir), files, error, content))
    return Edition::unknown;
  if (content.full_game.empty()) return Edition::full;
  std::vector<uint8_t> store;
  return find_file(files, content.full_game, store).empty() ? Edition::shareware : Edition::full;
}

std::string_view edition_words(Edition e, const ge::classicenhance::Profile& content) {
  const bool sequel = content.id != ge::classicenhance::classic_profile().id;
  switch (e) {
    case Edition::shareware:
      return sequel ? "Demo" : "Shareware";
    case Edition::full:
      return sequel ? "Full game" : "Registered";
    case Edition::unknown:
      break;
  }
  return "";
}

}  // namespace shooters_edge::client
