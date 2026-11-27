// The game played and each classic game's install as settings, and their editions (installs.hpp).
//
// ge:summary The game played and each game's install as settings; an install's edition
// ge:keywords installs fs_basegame fs_basedir bind_base_game read_base_game edition_of shareware
// ge:keywords registered
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#include "shooters_edge/client/installs.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

#include "classic/content/load_order.hpp"
#include "shooters_edge/client/cvar_layers.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "vfs/search.hpp"

namespace shooters_edge::client {

namespace fs = std::filesystem;

namespace {

// A config line's or the command line's words: quoted words whole, `//` ending the line.
std::vector<std::string> words_of(std::string_view line) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) i++;
    if (i >= line.size() || line.compare(i, 2, "//") == 0) break;
    std::string w;
    if (line[i] == '"') {
      const size_t end = line.find('"', i + 1);
      w = std::string(line.substr(i + 1, end == std::string_view::npos ? end : end - i - 1));
      i = end == std::string_view::npos ? line.size() : end + 1;
    } else {
      while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
        w += line[i++];
    }
    out.push_back(std::move(w));
  }
  return out;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

// `name value` into `base` when `name` is one of its settings; false when not.
bool take_setting(BaseGame& base, std::string_view name, const std::string& value) {
  const std::string n = lower(name);
  if (n == kBaseGame) {
    base.game = value;
    return true;
  }
  for (auto& [id, dir] : base.dirs)
    if (n == basedir_setting(id)) {
      dir = value;
      return true;
    }
  return false;
}

}  // namespace

std::string basedir_setting(std::string_view id) { return "fs_basedir_" + std::string(id); }

BaseGame::BaseGame() {
  for (const ge::classicenhance::Profile& p : ge::classicenhance::kProfiles)
    dirs[std::string(p.id)];
}

std::string BaseGame::dir(std::string_view id) const {
  const auto it = dirs.find(id);
  return it != dirs.end() ? resolve_basedir(it->second, from) : std::string();
}

std::string resolve_basedir(const std::string& value, const std::string& from) {
  if (value.empty()) return value;
  const fs::path p(expand_home(value));
  if (p.is_absolute() || from.empty()) return p.string();
  return (fs::path(from) / p).lexically_normal().string();
}

std::string set_base_game(ge::console::Cvars& cvars, const BaseGame& base) {
  for (const auto& [id, dir] : base.dirs)
    if (!dir.empty()) cvars.set(basedir_setting(id), dir, ge::console::Source::code);
  cvars.set(std::string(kBaseGame), base.game, ge::console::Source::code);
  return base.game + "\n" + base.dir(base.game);
}

BaseGame bind_base_game(ge::console::Cvars& cvars, BaseGame* out) {
  const BaseGame held = *out;                        // binding writes the fallbacks
  ge::console::CvarLayer shell(cvars, kLayerShell);  // every game's (cvar_layers.hpp)
  std::string ids = "\"\": Shooter's Edge's own game (the default)";
  for (const ge::classicenhance::Profile& p : ge::classicenhance::kProfiles)
    ids += "; " + std::string(p.id) + ": " + std::string(p.what);
  cvars.bind({.name = std::string(kBaseGame),
              .fallback = "",
              .flags = ge::console::kArchive,
              .help = "The game you play. Its install is the folder its own fs_basedir setting "
                      "names; setting this opens it.",
              .kind = ge::console::CvarKind::text,
              .values = ids},
             &out->game);
  for (const ge::classicenhance::Profile& p : ge::classicenhance::kProfiles)
    cvars.bind(
        {.name = basedir_setting(p.id),
         .fallback = "",
         .flags = ge::console::kArchive,
         .help = "The folder " + std::string(p.what) + " is installed in: the one holding its " +
                 std::string(p.base_dir) + " folder. Opening the game sets it.",
         .kind = ge::console::CvarKind::text,
         .values = "\"\": not added (the default); FOLDER: that install"},
        &out->dirs[std::string(p.id)]);
  set_base_game(cvars, held);
  return held;
}

void keep_base_game(ge::console::Cvars& cvars, const BaseGame& base, std::string_view id,
                    const std::string& basedir, std::string* seen) {
  cvars.set(basedir_setting(id), basedir, ge::console::Source::code);
  cvars.set(std::string(kBaseGame), std::string(id), ge::console::Source::code);
  *seen = base.game + "\n" + base.dir(base.game);
}

void ask_base_game(const BaseGame& base, std::string* seen, std::string* folder,
                   ge::console::Console& con) {
  std::string why;
  if (std::string asked = base_game_asked(base, seen, &why); !asked.empty())
    *folder = std::move(asked);
  else if (!why.empty())
    con.print(why);
}

std::string base_game_asked(const BaseGame& base, std::string* seen, std::string* why) {
  why->clear();
  const std::string now = base.game + "\n" + base.dir(base.game);
  if (now == *seen) return "";
  *seen = now;
  const ge::classicenhance::Profile* p = ge::classicenhance::find_profile(base.game);
  if (base.game.empty()) {
    *why = "Shooter's Edge's own game opens at the next start.";
  } else if (p == nullptr) {
    *why = std::string(kBaseGame) + " " + base.game + " is no game here. The games are " +
           ge::classicenhance::profile_ids() + ".";
  } else if (base.dir(base.game).empty()) {
    *why = std::string(p->what) + " has no install set. Add it in SELECT GAME, or set " +
           basedir_setting(p->id) + " to its folder.";
  }
  return why->empty() ? base.dir(base.game) : "";
}

BaseGame read_base_game(std::string_view config, const std::vector<std::string>& plus) {
  BaseGame base;
  size_t at = 0;
  while (at < config.size()) {
    const size_t nl = config.find('\n', at);
    const std::string_view line =
        config.substr(at, nl == std::string_view::npos ? std::string_view::npos : nl - at);
    at = nl == std::string_view::npos ? config.size() : nl + 1;
    std::vector<std::string> w = words_of(line);
    if (!w.empty() && (lower(w[0]) == "set" || lower(w[0]) == "seta")) w.erase(w.begin());
    if (w.size() >= 2) take_setting(base, w[0], w[1]);
  }
  for (size_t i = 0; i < plus.size(); i++) {
    const std::string w = lower(plus[i]);
    if ((w == "+set" || w == "+seta") && i + 2 < plus.size()) {
      if (take_setting(base, plus[i + 1], plus[i + 2])) i += 2;
    } else if (w.size() > 1 && w[0] == '+' && i + 1 < plus.size() &&
               take_setting(base, w.substr(1), plus[i + 1])) {
      i++;
    }
  }
  return base;
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

std::vector<shooters_edge::menu::GameRow> game_rows(const BaseGame& base, std::string_view playing,
                                                    const std::string& playing_install,
                                                    std::string_view remixed,
                                                    std::string_view playing_folder) {
  std::vector<shooters_edge::menu::GameRow> rows;
  for (const ge::classicenhance::Profile& p : ge::classicenhance::kProfiles) {
    shooters_edge::menu::GameRow g;
    g.id = std::string(p.id);
    g.install = base.dir(p.id);
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

BaseGame read_base_game(const std::vector<std::filesystem::path>& files,
                        const std::vector<std::string>& plus) {
  std::string text;
  for (const std::filesystem::path& file : files)
    if (std::ifstream f(file, std::ios::binary); f)
      text.append(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()) += '\n';
  return read_base_game(std::string_view(text), plus);
}

}  // namespace shooters_edge::client
