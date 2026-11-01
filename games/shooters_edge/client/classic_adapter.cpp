// The first game's adapter (game_adapter.hpp): its mods are folders over the base of numbered
// packs, zips and loose files with QC programs for the server, its maps carry their textures'
// pixels, and its demos are the recordings client/demo.hpp reads.
//
// ge:summary The first classic game's adapter: mod folders, QC programs, maps, pictures, demos
// ge:keywords classic-adapter list_mods find_program list_maps map_info map_textures list_demos
// ge:keywords find_demo open_demo mod-folder qwprogs replacement-texture png tga
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include <algorithm>
#include <filesystem>
#include <optional>
#include <set>
#include <system_error>
#include <utility>

#include "assets/image.hpp"
#include "assets/tga.hpp"
#include "brush/read.hpp"
#include "core/output.hpp"
#include "progs/layout.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "vfs/search.hpp"

namespace shooters_edge::client {

namespace {

namespace fs = std::filesystem;
namespace pf = shooters_edge::profiles;
namespace pg = ge::progs;

std::string folded(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
  return out;
}

// Every program file of the row, each name once, multiplayer's first.
std::vector<std::string_view> program_names(const pf::GameProfile& g) {
  std::vector<std::string_view> out;
  for (const auto list : {g.multiplayer_programs, g.single_player_programs})
    for (const pf::ProgramFile& f : list)
      if (std::find(out.begin(), out.end(), f.name) == out.end()) out.push_back(f.name);
  return out;
}

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
  const ge::classicenhance::AssetKind& maps = pf::content_of(*self.game).maps;
  const std::string path = ge::classicenhance::asset_path(maps, name);
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, path, store);
  if (bytes.empty()) {
    error = "no " + path + " in the game's files; next: `maps` lists the maps there are";
    return false;
  }
  ge::brush::Map m;
  if (!ge::brush::read_bsp(bytes, &m, &error)) {
    error = path + ": " + error;
    return false;
  }
  out.name = std::string(name);
  out.title = m.entities.empty() ? std::string() : m.entities[0].get("message");
  out.family = m.family;
  out.textures = static_cast<uint32_t>(m.textures.size());
  return true;
}

// A PNG or TGA in the files standing in for the texture `name` of map `map`, read into `p`.
void replace_picture(const SearchPath& files, std::string_view map, ge::brush::MapPicture& p) {
  std::string name = p.name;
  std::replace(name.begin(), name.end(), '*', '#');
  const std::string places[] = {"textures/" + std::string(map) + "/" + name, "textures/" + name};
  for (const std::string& stem : places) {
    for (const char* ext : {".png", ".tga"}) {
      const std::string path = stem + ext;
      std::vector<uint8_t> store;
      const std::span<const uint8_t> bytes = find_file(files, path, store);
      if (bytes.empty()) continue;
      std::string why;
      std::optional<ge::assets::Image> img =
          ext[1] == 'p' ? ge::assets::decode_png(bytes) : ge::assets::decode_tga(bytes, &why);
      if (!img || img->width <= 0 || img->height <= 0) {
        ge::core::output::status("shooters_edge: " + path + " did not read" +
                                 (why.empty() ? std::string() : " (" + why + ")") +
                                 "; the map's own " + p.name + " is drawn");
        continue;
      }
      p.rgba = std::move(img->rgba);
      p.rgba_width = static_cast<uint32_t>(img->width);
      p.rgba_height = static_cast<uint32_t>(img->height);
      return;
    }
  }
}

std::vector<ge::brush::MapPicture> map_textures(const GameAdapter&, const SearchPath& files,
                                                std::string_view map,
                                                std::span<const uint8_t> map_file,
                                                std::vector<uint8_t>&) {
  // The first game's maps carry their pictures; a picture file of the same name stands in.
  std::vector<ge::brush::MapPicture> out = ge::brush::read_map_pictures(map_file);
  for (ge::brush::MapPicture& p : out)
    if (!p.name.empty()) replace_picture(files, map, p);
  return out;
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

bool find_demo_(const GameAdapter& self, const SearchPath& files, const std::string& name,
                std::vector<uint8_t>& out, std::string& found, std::string& error) {
  return find_demo(files, name, self.game->demo_extensions, out, found, error);
}

bool open_demo_(const GameAdapter&, std::vector<uint8_t> bytes, Demo& out, std::string& error) {
  return open_demo(std::move(bytes), out, error);
}

const GameAdapter kClassic{.game = &pf::kGameProfiles[0],
                           .list_mods = list_mods,
                           .find_program = find_program,
                           .list_maps = list_maps,
                           .map_info = map_info,
                           .map_textures = map_textures,
                           .list_demos = list_demos,
                           .find_demo = find_demo_,
                           .open_demo = open_demo_};

}  // namespace

const GameAdapter& classic_adapter() { return kClassic; }

}  // namespace shooters_edge::client
