// The open calls every shooter game shares (open.hpp).
//
// ge:summary The shared open calls: a model by its kind, a sound, map textures, configs, places
// ge:keywords content open_model open_sound map_textures map_info find_config open_locations
// ge:keywords stand_in skeletal iqm mdl md2 spr sp2 wal replacement-texture
// ge:skill classic shooter
// ge:test tests/classic_content_open_test.cpp
#include "classic/content/open.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <utility>

#include "assets/iqm.hpp"
#include "assets/pcx.hpp"
#include "assets/tga.hpp"
#include "brush/read.hpp"
#include "brush/wal.hpp"
#include "core/output.hpp"
#include "morph/read.hpp"
#include "morph/skeletal.hpp"

namespace ge::classic::content {

namespace fs = std::filesystem;

std::string folded(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
  return out;
}

// The first picture file of `stems` the files hold, each tried as .png then .tga, read; one that
// will not read is named (`own` says what is drawn instead) and the next is tried.
std::optional<assets::Image> stand_in(const SearchPath& files, std::span<const std::string> stems,
                                      const std::string& own) {
  for (const std::string& stem : stems) {
    for (const char* ext : {".png", ".tga"}) {
      const std::string path = stem + ext;
      std::vector<uint8_t> store;
      const std::span<const uint8_t> bytes = find_file(files, path, store);
      if (bytes.empty()) continue;
      std::string why;
      std::optional<assets::Image> img =
          ext[1] == 'p' ? assets::decode_png(bytes) : assets::decode_tga(bytes, &why);
      if (!img || img->width <= 0 || img->height <= 0) {
        std::string line = "content: " + path + " did not read";
        if (!why.empty()) line.append(" (").append(why).append(")");
        line.append("; ").append(own).append(" is drawn");
        core::output::status(line);
        continue;
      }
      return img;
    }
  }
  return std::nullopt;
}

namespace {

// A PNG or TGA in the files standing in for the texture `p.name` of map `map`, read into `p`.
void replace_picture(const SearchPath& files, std::string_view map, brush::MapPicture& p) {
  std::string name = p.name;
  std::replace(name.begin(), name.end(), '*', '#');
  const std::string places[] = {"textures/" + std::string(map) + "/" + name, "textures/" + name};
  std::optional<assets::Image> img = stand_in(files, places, "the map's own " + p.name);
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
  const assets::LoadResult read = assets::load_iqm(bytes, name);
  if (!read.ok) {
    error = read.error;
    return false;
  }
  const fs::path dir = fs::path(name).parent_path();
  std::vector<assets::Image> pictures;
  int width = 0, height = 0;
  for (const assets::Material& mat : read.model.materials) {
    const std::string stem = fs::path(mat.name).replace_extension().generic_string();
    const std::string stems[] = {(dir / stem).generic_string(), stem, "textures/" + stem};
    std::optional<assets::Image> img =
        mat.name.empty() ? std::nullopt : stand_in(files, stems, "grey for " + mat.name);
    if (!img) {
      img = assets::Image{8, 8, {}};
      img->rgba.assign(size_t{256}, 128);
    }
    width = std::max(width, img->width);
    height += img->height;
    pictures.push_back(std::move(*img));
  }
  std::vector<std::array<float, 4>> rects;
  assets::Image atlas{width, height, std::vector<uint8_t>(size_t(width) * height * 4, 0)};
  int y = 0;
  for (const assets::Image& img : pictures) {
    for (int r = 0; r < img.height; ++r)
      std::copy_n(img.rgba.begin() + ptrdiff_t(r) * img.width * 4, size_t(img.width) * 4,
                  atlas.rgba.begin() + (ptrdiff_t(y + r) * width) * 4);
    rects.push_back({0, float(y) / float(height), float(img.width) / float(width),
                     float(y + img.height) / float(height)});
    y += img.height;
  }
  if (!morph::bake_skeletal(read.model, name, {.rects = rects}, &out.morph, &error)) return false;
  out.form = ModelForm::morph;
  out.skins.clear();
  if (width > 0 && height > 0) out.skins.push_back(std::move(atlas));
  return true;
}

// A picture the second model family names (a skin, a sprite's frame) read as palette indices; an
// unreadable or missing one says so and is left out.
bool named_picture(const SearchPath& files, const std::string& path, const std::string& model,
                   assets::IndexedImage& out) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, path, store);
  std::string why;
  if (bytes.empty() || !assets::decode_pcx(bytes, path, &out, &why)) {
    core::output::status("content: " + model + "'s picture " + path +
                         (bytes.empty() ? " is not in the files" : " did not read (" + why + ")") +
                         "; it is drawn without it");
    return false;
  }
  return true;
}

}  // namespace

bool map_info(const SearchPath& files, const classicenhance::Profile& content,
              std::string_view name, MapInfo& out, std::string& error) {
  const classicenhance::AssetKind& maps = content.maps;
  const std::string path = classicenhance::asset_path(maps, name);
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, path, store);
  if (bytes.empty()) {
    error = "There is no " + path + " in the game's files. Type \"maps\" to list the maps.";
    return false;
  }
  brush::Map m;
  if (!brush::read_bsp(bytes, &m, &error)) {
    error = path + ": " + error;
    return false;
  }
  out.name = std::string(name);
  out.title = m.entities.empty() ? std::string() : m.entities[0].get("message");
  out.family = m.family;
  out.textures = static_cast<uint32_t>(m.textures.size());
  return true;
}

bool open_model(const SearchPath& files, const std::string& name, OpenedModel& out,
                std::string& error) {
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
    if (!morph::read_model(bytes, name, &out.morph, &error)) return false;
    out.form = ModelForm::morph;
    // A picture beside the model stands in for each of its skins: "<model>_<skin>".
    out.skins.resize(out.morph.skins.size());
    for (size_t s = 0; s < out.skins.size(); s++) {
      const std::string stem[] = {name + "_" + std::to_string(s)};
      std::optional<assets::Image> img =
          stand_in(files, stem, "the model's own skin " + std::to_string(s));
      if (img) out.skins[s] = std::move(*img);
    }
    return true;
  }
  if (kind == ".md2") {
    if (!morph::read_model(bytes, name, &out.morph, &error)) return false;
    out.form = ModelForm::morph;
    // Its skins are the game's pictures, by name: read into the model as palette indices.
    for (const std::string& path : out.morph.skin_paths) {
      assets::IndexedImage img;
      if (!named_picture(files, path, name, img)) continue;
      out.morph.skins.push_back(
          {img.width, img.height, std::move(img.indices), 0.0f, int(out.morph.skins.size())});
    }
    return true;
  }
  if (kind == ".spr" || kind == ".sp2") {
    if (!morph::read_sprite(bytes, name, &out.sprite, &error)) return false;
    out.form = ModelForm::sprite;
    for (morph::SpriteFrame& f : out.sprite.frames) {
      assets::IndexedImage img;
      if (f.picture.empty() || !named_picture(files, f.picture, name, img)) continue;
      f.width = img.width;
      f.height = img.height;
      f.indices = std::move(img.indices);
    }
    // A frame whose picture is missing is dropped; a sprite with none left is not drawn.
    std::erase_if(out.sprite.frames, [](const morph::SpriteFrame& f) { return f.indices.empty(); });
    if (out.sprite.frames.empty()) {
      error = "none of its frames' pictures read";
      return false;
    }
    return true;
  }
  if (kind == ".bsp") {
    if (!brush::read_bsp(bytes, &out.brush, &error)) return false;
    out.form = ModelForm::brush;
    return true;
  }
  error = "a model of a kind this game does not draw (" + kind +
          "); use a .mdl, .md2, .iqm, .spr, .sp2 or .bsp";
  return false;
}

bool open_sound(const SearchPath& files, const classicenhance::Profile& content,
                const std::string& name, audio::Pcm& out, std::string& error) {
  error.clear();
  const std::string path = std::string(content.sounds.dir) + name;
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, path, store);
  if (bytes.empty()) return false;
  return audio::read_wav(bytes, path, &out, &error);
}

bool find_config(const SearchPath& files, std::string_view name, std::string& text) {
  if (name.empty() || name.find("..") != std::string_view::npos) return false;  // in the files
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, name, store);
  if (bytes.empty()) return false;
  text.assign(bytes.begin(), bytes.end());
  return true;
}

bool open_locations(const SearchPath& files, std::string_view map, ge::classic::Locs& out) {
  std::string path = "locs/";
  path.append(map).append(".loc");
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, path, store);
  if (bytes.empty()) return false;
  const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  if (ge::classic::parse_locs(text, out) == 0) return false;
  out.from = path;
  return true;
}

std::vector<brush::MapPicture> map_textures(const SearchPath& files, std::string_view map,
                                            std::span<const uint8_t> map_file,
                                            std::vector<uint8_t>& store) {
  // A map that carries its pictures (29, BSP2) gives them; any other family's are its wall
  // texture files, found by the textures' names in whichever game's files are open, so a map of
  // one game's family draws in another's folder. A picture file of the same name stands in.
  std::vector<brush::MapPicture> out;
  if (brush::carries_pictures(map_file)) {
    out = brush::read_map_pictures(map_file);
  } else {
    brush::Map m;
    std::string error;
    if (!brush::read_bsp(map_file, &m, &error)) return out;
    // Every file's bytes first (a loose file's copied into `store`, which then never grows
    // again), then the views into them.
    std::vector<std::span<const uint8_t>> views(m.textures.size());
    std::vector<std::pair<size_t, size_t>> copied(m.textures.size(), {SIZE_MAX, 0});
    for (size_t i = 0; i < m.textures.size(); ++i) {
      std::vector<uint8_t> one;
      const std::span<const uint8_t> bytes =
          find_file(files, brush::wal_path(m.textures[i].name), one);
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
    out = brush::wal_pictures(m, views, &faults);
    if (!faults.empty())
      core::output::status("content: " + std::to_string(faults.size()) + " of " +
                           std::to_string(m.textures.size()) + " textures of " + std::string(map) +
                           " are drawn as checks (the first: " + faults[0] +
                           "); check that the game's packs are whole");
  }
  for (brush::MapPicture& p : out)
    if (!p.name.empty()) replace_picture(files, map, p);
  return out;
}

}  // namespace ge::classic::content
