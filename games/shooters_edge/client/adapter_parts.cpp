// The calls every classic game's adapter shares (adapter_parts.hpp).
//
// ge:summary The shared adapter calls: mods, programs, maps, wall textures, models, sounds, demos
// ge:keywords adapter-parts list_mods find_program map_textures wal open_model open_sound
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include "shooters_edge/client/adapter_parts.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <set>
#include <system_error>
#include <utility>

#include "assets/image.hpp"
#include "assets/iqm.hpp"
#include "assets/tga.hpp"
#include "brush/pictures.hpp"
#include "brush/read.hpp"
#include "brush/wal.hpp"
#include "core/output.hpp"
#include "morph/read.hpp"
#include "morph/skeletal.hpp"
#include "progs/layout.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "vfs/search.hpp"

namespace shooters_edge::client::parts {

namespace fs = std::filesystem;
namespace pf = shooters_edge::profiles;
namespace pg = ge::progs;

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

std::string folded(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
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

// The first picture file of `stems` the files hold, each tried as .png then .tga, read; one that
// will not read is named (`own` says what is drawn instead) and the next is tried.
std::optional<ge::assets::Image> stand_in(const SearchPath& files,
                                          std::span<const std::string> stems,
                                          const std::string& own) {
  for (const std::string& stem : stems) {
    for (const char* ext : {".png", ".tga"}) {
      const std::string path = stem + ext;
      std::vector<uint8_t> store;
      const std::span<const uint8_t> bytes = find_file(files, path, store);
      if (bytes.empty()) continue;
      std::string why;
      std::optional<ge::assets::Image> img =
          ext[1] == 'p' ? ge::assets::decode_png(bytes) : ge::assets::decode_tga(bytes, &why);
      if (!img || img->width <= 0 || img->height <= 0) {
        std::string line = "shooters_edge: " + path + " did not read";
        if (!why.empty()) line.append(" (").append(why).append(")");
        line.append("; ").append(own).append(" is drawn");
        ge::core::output::status(line);
        continue;
      }
      return img;
    }
  }
  return std::nullopt;
}

namespace {

// A PNG or TGA in the files standing in for the texture `p.name` of map `map`, read into `p`.
void replace_picture(const SearchPath& files, std::string_view map, ge::brush::MapPicture& p) {
  std::string name = p.name;
  std::replace(name.begin(), name.end(), '*', '#');
  const std::string places[] = {"textures/" + std::string(map) + "/" + name, "textures/" + name};
  std::optional<ge::assets::Image> img = stand_in(files, places, "the map's own " + p.name);
  if (!img) return;
  p.rgba = std::move(img->rgba);
  p.rgba_width = static_cast<uint32_t>(img->width);
  p.rgba_height = static_cast<uint32_t>(img->height);
}

// A mod's skeletal model (`.iqm`): read, posed through its clips into frames (a clip a frame
// group, morph/skeletal.hpp), its materials' pictures laid one under another in one skin, each
// found as the material's name (its extension dropped) beside the model, at the files' root, or
// under `textures/`; a material with no picture is the palette's grey.
bool open_skeletal(const SearchPath& files, const std::string& name, std::span<const uint8_t> bytes,
                   OpenedModel& out, std::string& error) {
  const ge::assets::LoadResult read = ge::assets::load_iqm(bytes, name);
  if (!read.ok) {
    error = read.error;
    return false;
  }
  const fs::path dir = fs::path(name).parent_path();
  std::vector<ge::assets::Image> pictures;
  int width = 0, height = 0;
  for (const ge::assets::Material& mat : read.model.materials) {
    const std::string stem = fs::path(mat.name).replace_extension().generic_string();
    const std::string stems[] = {(dir / stem).generic_string(), stem, "textures/" + stem};
    std::optional<ge::assets::Image> img =
        mat.name.empty() ? std::nullopt : stand_in(files, stems, "grey for " + mat.name);
    if (!img) {
      img = ge::assets::Image{8, 8, {}};
      img->rgba.assign(size_t{256}, 128);
    }
    width = std::max(width, img->width);
    height += img->height;
    pictures.push_back(std::move(*img));
  }
  std::vector<std::array<float, 4>> rects;
  ge::assets::Image atlas{width, height, std::vector<uint8_t>(size_t(width) * height * 4, 0)};
  int y = 0;
  for (const ge::assets::Image& img : pictures) {
    for (int r = 0; r < img.height; ++r)
      std::copy_n(img.rgba.begin() + ptrdiff_t(r) * img.width * 4, size_t(img.width) * 4,
                  atlas.rgba.begin() + (ptrdiff_t(y + r) * width) * 4);
    rects.push_back({0, float(y) / float(height), float(img.width) / float(width),
                     float(y + img.height) / float(height)});
    y += img.height;
  }
  if (!ge::morph::bake_skeletal(read.model, name, {.rects = rects}, &out.morph, &error))
    return false;
  out.form = ModelForm::morph;
  out.skins.clear();
  if (width > 0 && height > 0) out.skins.push_back(std::move(atlas));
  return true;
}

}  // namespace

bool open_model(const GameAdapter&, const SearchPath& files, const std::string& name,
                OpenedModel& out, std::string& error) {
  out = {};
  error.clear();
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, name, store);
  if (bytes.empty()) return false;
  const std::string kind = folded(fs::path(name).extension().string());
  // A skeletal model by its name, or by its first bytes under another name (a mod's stand-in).
  constexpr char kSkeletal[16] = "INTERQUAKEMODEL";
  if (kind == ".iqm" || (bytes.size() >= 16 && std::memcmp(bytes.data(), kSkeletal, 16) == 0))
    return open_skeletal(files, name, bytes, out, error);
  if (kind == ".mdl") {
    if (!ge::morph::read_model(bytes, name, &out.morph, &error)) return false;
    out.form = ModelForm::morph;
    // A picture beside the model stands in for each of its skins: "<model>_<skin>".
    out.skins.resize(out.morph.skins.size());
    for (size_t s = 0; s < out.skins.size(); s++) {
      const std::string stem[] = {name + "_" + std::to_string(s)};
      std::optional<ge::assets::Image> img =
          stand_in(files, stem, "the model's own skin " + std::to_string(s));
      if (img) out.skins[s] = std::move(*img);
    }
    return true;
  }
  if (kind == ".spr") {
    if (!ge::morph::read_sprite(bytes, name, &out.sprite, &error)) return false;
    out.form = ModelForm::sprite;
    return true;
  }
  if (kind == ".bsp") {
    if (!ge::brush::read_bsp(bytes, &out.brush, &error)) return false;
    out.form = ModelForm::brush;
    return true;
  }
  error = "a model of a kind this game does not draw (" + kind +
          "); next: name a .mdl, .iqm, .spr or .bsp";
  return false;
}

bool open_sound(const GameAdapter& self, const SearchPath& files, const std::string& name,
                ge::audio::Pcm& out, std::string& error) {
  error.clear();
  const std::string path = std::string(pf::content_of(*self.game).sounds.dir) + name;
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, path, store);
  if (bytes.empty()) return false;
  return ge::audio::read_wav(bytes, path, &out, &error);
}

bool find_config(const GameAdapter&, const SearchPath& files, std::string_view name,
                 std::string& text) {
  if (name.empty() || name.find("..") != std::string_view::npos) return false;  // in the files
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, name, store);
  if (bytes.empty()) return false;
  text.assign(bytes.begin(), bytes.end());
  return true;
}

bool open_locations(const GameAdapter&, const SearchPath& files, std::string_view map,
                    ge::shooter::Locs& out) {
  std::string path = "locs/";
  path.append(map).append(".loc");
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, path, store);
  if (bytes.empty()) return false;
  const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  if (ge::shooter::parse_locs(text, out) == 0) return false;
  out.from = path;
  return true;
}

GameModes modes(const GameAdapter& self) {
  return {.single = !self.game->single_player_programs.empty(),
          .multi = !self.game->multiplayer_programs.empty()};
}

std::vector<ge::brush::MapPicture> map_textures(const GameAdapter&, const SearchPath& files,
                                                std::string_view map,
                                                std::span<const uint8_t> map_file,
                                                std::vector<uint8_t>& store) {
  // A map that carries its pictures (29, BSP2) gives them; any other family's are its wall
  // texture files, found by the textures' names in whichever game's files are open, so a map of
  // one game's family draws in another's folder. A picture file of the same name stands in.
  std::vector<ge::brush::MapPicture> out;
  if (ge::brush::carries_pictures(map_file)) {
    out = ge::brush::read_map_pictures(map_file);
  } else {
    ge::brush::Map m;
    std::string error;
    if (!ge::brush::read_bsp(map_file, &m, &error)) return out;
    // Every file's bytes first (a loose file's copied into `store`, which then never grows
    // again), then the views into them.
    std::vector<std::span<const uint8_t>> views(m.textures.size());
    std::vector<std::pair<size_t, size_t>> copied(m.textures.size(), {SIZE_MAX, 0});
    for (size_t i = 0; i < m.textures.size(); ++i) {
      std::vector<uint8_t> one;
      const std::span<const uint8_t> bytes =
          find_file(files, ge::brush::wal_path(m.textures[i].name), one);
      if (bytes.empty()) continue;
      if (one.empty()) {
        views[i] = bytes;  // a view into a pack
      } else {
        copied[i] = {store.size(), one.size()};
        store.insert(store.end(), one.begin(), one.end());
      }
    }
    for (size_t i = 0; i < copied.size(); ++i)
      if (copied[i].first != SIZE_MAX)
        views[i] = std::span<const uint8_t>(store).subspan(copied[i].first, copied[i].second);
    std::vector<std::string> faults;
    out = ge::brush::wal_pictures(m, views, &faults);
    if (!faults.empty())
      ge::core::output::status("shooters_edge: " + std::to_string(faults.size()) + " of " +
                               std::to_string(m.textures.size()) + " textures of " +
                               std::string(map) + " are drawn as checks (the first: " + faults[0] +
                               "); next: check the game's packs are whole");
  }
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

bool find_demo(const GameAdapter& self, const SearchPath& files, const std::string& name,
               std::vector<uint8_t>& out, std::string& found, std::string& error) {
  return client::find_demo(files, name, self.game->demo_extensions, out, found, error);
}

}  // namespace shooters_edge::client::parts
