// Each classic game's install kept by game, and its edition (installs.hpp).
//
// ge:summary Keeps each classic game's install and reads its edition off its packs
// ge:keywords installs remember_install remembered_installs edition_of shareware registered
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#include "shooters_edge/client/installs.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

#include "classic/content/load_order.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "vfs/search.hpp"

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
  // The places the base game's search path reads, asked by their directories alone: mounting
  // them read every pack whole, seconds at start-up for each install known.
  const fs::path root = basedir.empty() ? "." : basedir;
  const std::string base(content.base_dir);
  std::error_code ec;
  if (!fs::is_directory(root / base, ec)) return Edition::unknown;
  if (content.full_game.empty()) return Edition::full;
  const auto& order = ge::classic::content::load_order_of(content);
  const std::string rootdir = root.string();
  const bool full =
      ge::vfs::game_has_file(rootdir, base, content.full_game, order.layout) ||
      (!order.root_archive.empty() &&
       ge::vfs::archive_has((root / std::string(order.root_archive)).string(),
                            content.full_game)) ||
      [&] {
        const fs::path client = client_folder(root, content);
        return !client.empty() && ge::vfs::game_has_file(rootdir, client.filename().string(),
                                                         content.full_game, order.layout);
      }();
  return full ? Edition::full : Edition::shareware;
}

std::string_view edition_words(Edition e, const ge::classicenhance::Profile& content) {
  const bool q2 = content.id != ge::classicenhance::q1_profile().id;
  switch (e) {
    case Edition::shareware:
      return q2 ? "Demo" : "Shareware";
    case Edition::full:
      return q2 ? "Full game" : "Registered";
    case Edition::unknown:
      break;
  }
  return "";
}

std::string folder_over(const std::string& basedir, const ge::classicenhance::Profile& content,
                        std::string* pack) {
  const std::string asked = std::exchange(*pack, std::string());
  return !asked.empty() && ge::classicenhance::find_pack(content, asked) != nullptr
             ? asked
             : game_of(basedir, content);
}

std::vector<const ge::classicenhance::MissionPack*> packs_of(
    const std::string& basedir, const ge::classicenhance::Profile& content) {
  std::vector<const ge::classicenhance::MissionPack*> out;
  std::error_code ec;
  for (const ge::classicenhance::MissionPack& m : content.packs) {
    const std::filesystem::path dir = std::filesystem::path(basedir) / std::string(m.folder);
    if (std::filesystem::is_regular_file(dir / ge::classicenhance::pack_name(content, 0), ec) ||
        std::filesystem::is_regular_file(dir / ge::classicenhance::zip_name(content, 0), ec))
      out.push_back(&m);
  }
  return out;
}

std::vector<shooters_edge::menu::GameRow> game_rows(const std::string& prefs,
                                                    std::string_view playing,
                                                    const std::string& playing_install,
                                                    std::string_view remixed,
                                                    std::string_view playing_folder) {
  std::vector<shooters_edge::menu::GameRow> rows;
  const auto kept = remembered_installs(prefs);
  for (const ge::classicenhance::Profile& p : ge::classicenhance::kProfiles) {
    shooters_edge::menu::GameRow g;
    g.id = std::string(p.id);
    if (const auto it = kept.find(p.id); it != kept.end()) g.install = it->second;
    if (playing == p.id && !playing_install.empty()) {
      g.playing = true;
      g.install = playing_install;
    }
    // Only a game added here (ADD GAME, -basedir, a folder loaded) whose folder still holds it.
    if (g.install.empty()) continue;
    std::string basedir, why;
    const bool there = check_game_folder(g.install, &basedir, &why, p);
    if (there)
      g.edition = std::string(edition_words(edition_of(basedir, p), p));
    else if (!g.playing)
      continue;
    g.remixed = !g.playing && remixed == p.id;
    const bool on = g.playing;
    const auto packs = there ? packs_of(basedir, p) : decltype(packs_of(basedir, p)){};
    const auto pack_playing = [&](const ge::classicenhance::MissionPack* m) {
      return on && ge::classicenhance::find_pack(p, playing_folder) == m;
    };
    g.playing = on && std::none_of(packs.begin(), packs.end(), pack_playing);
    const std::string install = g.install;
    rows.push_back(std::move(g));
    for (const ge::classicenhance::MissionPack* m : packs) {
      shooters_edge::menu::GameRow r;
      r.id = std::string(p.id);
      r.install = install;
      r.pack = std::string(m->folder);
      r.pack_name = std::string(m->name);
      r.playing = pack_playing(m);
      rows.push_back(std::move(r));
    }
  }
  return rows;
}

}  // namespace shooters_edge::client
