// The calls every classic game's adapter shares (adapter_parts.hpp).
//
// ge:summary The shared adapter calls: mods, programs, maps, demos; the open calls forwarded
// ge:keywords adapter-parts list_mods find_program map_textures open_model open_sound
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include "shooters_edge/client/adapter_parts.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <set>
#include <system_error>
#include <utility>

#include "classic/content/open.hpp"
#include "progs/layout.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "vfs/search.hpp"

namespace shooters_edge::client::parts {

namespace fs = std::filesystem;
namespace pf = shooters_edge::profiles;
namespace pg = ge::progs;
namespace content = ge::classic::content;

namespace {

// Every program file of the row, each name once, multiplayer's first.
std::vector<std::string_view> program_names(const pf::GameProfile& g) {
  std::vector<std::string_view> out;
  for (const auto list : {g.multiplayer_programs, g.single_player_programs})
    for (const pf::ProgramFile& f : list)
      if (std::find(out.begin(), out.end(), f.name) == out.end()) out.push_back(f.name);
  return out;
}

}  // namespace

std::vector<ModFolder> list_mods(const GameAdapter& self, const std::string& basedir) {
  std::vector<ModFolder> out;
  const std::string base = folded(pf::content_of(*self.game).base_dir);
  std::error_code ec;
  std::vector<std::string> folders;
  for (const fs::directory_entry& e : fs::directory_iterator(basedir, ec)) {
    if (!e.is_directory(ec)) continue;
    const std::string name = e.path().filename().string();
    if (folded(name) != base) folders.push_back(name);
  }
  std::sort(folders.begin(), folders.end());
  const std::vector<std::string_view> programs = program_names(*self.game);
  for (const std::string& folder : folders) {
    ge::vfs::SearchPath sp;
    std::string error;
    if (!ge::vfs::add_game(&sp, basedir, folder, &error)) continue;  // an archive that won't read
    ModFolder m;
    m.folder = folder;
    for (const ge::vfs::Source& s : sp.sources)
      if (s.kind != ge::vfs::SourceKind::directory) m.archives++;
    for (const std::string_view p : programs)
      if (ge::vfs::find(sp, p) != nullptr) m.programs.emplace_back(p);
    if (m.archives > 0 || !m.programs.empty()) out.push_back(std::move(m));
  }
  return out;
}

bool find_program(const GameAdapter& self, const SearchPath& files, bool single, FoundProgram& out,
                  std::string& why) {
  why.clear();
  const auto pass = [&](std::string_view name, const std::string& reason) {
    why += (why.empty() ? "" : "; ") + std::string(name) + reason;
  };
  for (const pf::ProgramFile& file :
       single ? self.game->single_player_programs : self.game->multiplayer_programs) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = find_file(files, file.name, store);
    if (bytes.empty()) continue;
    if (!runs_form(file.form)) {
      pass(file.name, " is a " + std::string(pf::form_name(file.form)) +
                          " program, which this build has no runner for");
      continue;
    }
    pg::Progs p;
    std::string error;
    if (!pg::load_progs(bytes.data(), bytes.size(), p, error, pg::every_form())) {
      pass(file.name, " did not load (" + error + ")");
      continue;
    }
    if (single_player(p) != single) {
      pass(file.name,
           std::string(" is a ") + (single ? "multiplayer" : "single-player") + " program");
      continue;
    }
    out.file = file;
    out.progs = std::move(p);
    return true;
  }
  return false;
}

std::vector<std::string> list_maps(const GameAdapter& self, const SearchPath& files) {
  return map_names(files, pf::content_of(*self.game));
}

bool map_info(const GameAdapter& self, const SearchPath& files, std::string_view name, MapInfo& out,
              std::string& error) {
  return content::map_info(files, pf::content_of(*self.game), name, out, error);
}

bool open_model(const GameAdapter&, const SearchPath& files, const std::string& name,
                OpenedModel& out, std::string& error) {
  return content::open_model(files, name, out, error);
}

bool open_sound(const GameAdapter& self, const SearchPath& files, const std::string& name,
                ge::audio::Pcm& out, std::string& error) {
  return content::open_sound(files, pf::content_of(*self.game), name, out, error);
}

bool find_config(const GameAdapter&, const SearchPath& files, std::string_view name,
                 std::string& text) {
  return content::find_config(files, name, text);
}

bool open_locations(const GameAdapter&, const SearchPath& files, std::string_view map,
                    ge::classic::Locs& out) {
  return content::open_locations(files, map, out);
}

GameModes modes(const GameAdapter& self) {
  return {.single = !self.game->single_player_programs.empty(),
          .multi = !self.game->multiplayer_programs.empty()};
}

std::vector<ge::brush::MapPicture> map_textures(const GameAdapter&, const SearchPath& files,
                                                std::string_view map,
                                                std::span<const uint8_t> map_file,
                                                std::vector<uint8_t>& store) {
  return content::map_textures(files, map, map_file, store);
}

std::vector<std::string> list_demos(const GameAdapter& self, const SearchPath& files) {
  std::set<std::string> out;
  for (const ge::vfs::Listed& l : ge::vfs::list(files.vfs)) {
    for (const std::string_view ext : self.game->demo_extensions) {
      const std::string_view n = l.name;
      if (n.size() > ext.size() && n.substr(n.size() - ext.size()) == folded(ext)) {
        out.emplace(ge::vfs::stored_name(files.vfs, l.ref));
        break;
      }
    }
  }
  return {out.begin(), out.end()};
}

bool find_demo(const GameAdapter& self, const SearchPath& files, const std::string& name,
               std::vector<uint8_t>& out, std::string& found, std::string& error) {
  if (client::find_demo(files, name, self.game->demo_extensions, out, found, error)) return true;
  // A leading slash names the file from the top of the game's files, past its demo folder.
  if (name.size() > 1 && name[0] == '/') {
    std::string why;
    if (!client::find_demo(files, name.substr(1), self.game->demo_extensions, out, found, why))
      return false;
    error.clear();
    return true;
  }
  // A game that keeps its recordings in a folder of their own finds a bare name there too.
  if (self.game->demo_folder.empty() || name.find('/') != std::string::npos) return false;
  std::string why;
  if (!client::find_demo(files, std::string(self.game->demo_folder) + name,
                         self.game->demo_extensions, out, found, why))
    return false;
  error.clear();
  return true;
}

}  // namespace shooters_edge::client::parts
