// The cache read back by a game (cached.hpp).
//
// ge:summary Reads the enhanced cache back: the pack's folder, textures, skins and pictures by name
// ge:keywords classicenhance cache read find_cache cached_texture cached_skins albedo emissive
// ge:keywords cached_surface relief cached_picture pictures enhanced hud menu conchars
// ge:skill classicenhance
// ge:test tests/classicenhance_cached_test.cpp
#include "classicenhance/cached.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>

#include "assets/sha256.hpp"
#include "classicenhance/cache.hpp"
#include "lab/png.hpp"

namespace ge::classicenhance {

namespace fs = std::filesystem;

namespace {

// The multiple of w x h a picture is at (1, 2, 4 or 8); 0 when it is none of them.
uint32_t scale_of(const lab::Picture& p, uint32_t w, uint32_t h) {
  for (uint32_t k : {1u, 2u, 4u, 8u}) {
    if (uint32_t(p.width) == w * k && uint32_t(p.height) == h * k) return k;
  }
  return 0;
}

// `colour` with alpha 255 where `glow` (the same size) is lit, 0 elsewhere.
void mark_glow(lab::Picture& colour, const std::optional<lab::Picture>& glow) {
  const bool fits = glow && glow->width == colour.width && glow->height == colour.height;
  for (size_t i = 0; i + 3 < colour.rgba.size(); i += 4) {
    const bool lit =
        fits && (glow->rgba[i] > 24 || glow->rgba[i + 1] > 24 || glow->rgba[i + 2] > 24);
    colour.rgba[i + 3] = lit ? 255 : 0;
  }
}

// Whether the pack's own texture `name` is left to the pack (a liquid, the sky).
bool left_to_pack(std::string_view name) {
  return name.empty() || name[0] == '*' ||
         (name.size() >= 3 && (name[0] | 0x20) == 's' && (name[1] | 0x20) == 'k' &&
          (name[2] | 0x20) == 'y');
}

}  // namespace

fs::path find_cache(std::span<const uint8_t> pack, const fs::path& root) {
  const fs::path base = root.empty() ? default_cache_root() : root;
  if (base.empty() || pack.empty()) return {};
  const fs::path dir = pack_dir(base, assets::sha256_hex(pack));
  return recorded_key(dir, "textures").empty() ? fs::path{} : dir;
}

bool cached_texture(const fs::path& dir, std::string_view name, uint32_t w, uint32_t h,
                    std::vector<uint8_t>* rgba) {
  if (dir.empty() || left_to_pack(name)) return false;
  const fs::path at = dir / "textures" / safe_name(std::string(name));
  std::optional<lab::Picture> colour = lab::read_png((at / "albedo.png").string());
  if (!colour || scale_of(*colour, w, h) == 0) return false;
  std::error_code ec;
  const fs::path glow = at / "emissive.png";
  mark_glow(*colour, fs::exists(glow, ec) ? lab::read_png(glow.string()) : std::nullopt);
  *rgba = std::move(colour->rgba);
  return true;
}

bool cached_surface(const fs::path& dir, std::string_view name, uint32_t w, uint32_t h,
                    std::vector<uint8_t>* rgba) {
  if (dir.empty() || left_to_pack(name) || w == 0 || h == 0) return false;
  const fs::path at = dir / "textures" / safe_name(std::string(name));
  const std::optional<lab::Picture> normal = lab::read_png((at / "normal.png").string());
  const std::optional<lab::Picture> height = lab::read_png((at / "height.png").string());
  const std::optional<lab::Picture> rough = lab::read_png((at / "roughness.png").string());
  const auto fits = [&](const std::optional<lab::Picture>& p) {
    return p && uint32_t(p->width) == w && uint32_t(p->height) == h &&
           p->rgba.size() == size_t(w) * h * 4;
  };
  if (!normal || !height || !rough || !fits(normal) || !fits(height) || !fits(rough)) return false;
  const std::vector<uint8_t>& nv = normal->rgba;
  const std::vector<uint8_t>& hv = height->rgba;
  const std::vector<uint8_t>& rv = rough->rgba;
  rgba->resize(size_t(w) * h * 4);
  for (size_t i = 0; i < rgba->size(); i += 4) {
    (*rgba)[i] = nv[i];
    (*rgba)[i + 1] = nv[i + 1];
    (*rgba)[i + 2] = hv[i];
    (*rgba)[i + 3] = rv[i];
  }
  return true;
}

bool cached_skins(const fs::path& dir, std::string_view name, uint32_t w, uint32_t h,
                  std::vector<assets::Image>* skins) {
  if (dir.empty()) return false;
  const fs::path at = dir / "models" / safe_name(std::string(name));
  std::vector<assets::Image> out;
  std::error_code ec;
  for (int k = 0;; ++k) {
    const fs::path file = at / ("skin" + std::to_string(k) + ".png");
    if (!fs::exists(file, ec)) break;
    std::optional<lab::Picture> p = lab::read_png(file.string());
    if (!p || scale_of(*p, w, h) == 0) return false;
    const fs::path glow = at / ("glow" + std::to_string(k) + ".png");
    mark_glow(*p, fs::exists(glow, ec) ? lab::read_png(glow.string()) : std::nullopt);
    out.push_back({p->width, p->height, std::move(p->rgba)});
  }
  if (out.empty()) return false;
  *skins = std::move(out);
  return true;
}

bool cached_rebake(const fs::path& dir, std::string_view name, size_t vertices,
                   brush::Rebaked* out) {
  if (dir.empty() || name.empty()) return false;
  std::ifstream f(dir / "lightmaps" / (safe_name(std::string(name)) + ".gerb"), std::ios::binary);
  if (!f) return false;
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
  brush::Rebaked baked;
  std::string error;
  if (!brush::read_rebaked(bytes, &baked, &error) || baked.lm.size() != vertices * 2) return false;
  *out = std::move(baked);
  return true;
}

uint32_t cached_picture(const fs::path& dir, std::string_view name, uint32_t w, uint32_t h,
                        assets::Image* out) {
  if (dir.empty() || name.empty() || name.find("..") != std::string_view::npos) return 0;
  const fs::path file = dir / "pictures" / "enhanced" / (std::string(name) + ".png");
  std::error_code ec;
  if (!fs::exists(file, ec)) return 0;
  std::optional<lab::Picture> p = lab::read_png(file.string());
  if (!p) return 0;
  const uint32_t k = scale_of(*p, w, h);
  if (k == 0) return 0;
  *out = {p->width, p->height, std::move(p->rgba)};
  return k;
}

}  // namespace ge::classicenhance
