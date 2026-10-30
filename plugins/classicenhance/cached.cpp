// The cache read back by a game (cached.hpp).
//
// ge:summary Reads the enhanced cache back: the pack's folder, textures and skins by name
// ge:keywords classicenhance cache read find_cache cached_texture cached_skins albedo emissive
// ge:skill classicenhance
// ge:test tests/classicenhance_cached_test.cpp
#include "classicenhance/cached.hpp"

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

}  // namespace

fs::path find_cache(std::span<const uint8_t> pack, const fs::path& root) {
  const fs::path base = root.empty() ? default_cache_root() : root;
  if (base.empty() || pack.empty()) return {};
  const fs::path dir = pack_dir(base, assets::sha256_hex(pack));
  return recorded_key(dir, "textures").empty() ? fs::path{} : dir;
}

bool cached_texture(const fs::path& dir, std::string_view name, uint32_t w, uint32_t h,
                    std::vector<uint8_t>* rgba) {
  if (dir.empty() || name.empty() || name[0] == '*') return false;
  if (name.size() >= 3 && (name[0] | 0x20) == 's' && (name[1] | 0x20) == 'k' &&
      (name[2] | 0x20) == 'y')
    return false;
  const fs::path at = dir / "textures" / safe_name(std::string(name));
  std::optional<lab::Picture> colour = lab::read_png((at / "albedo.png").string());
  if (!colour || scale_of(*colour, w, h) == 0) return false;
  std::error_code ec;
  const fs::path glow = at / "emissive.png";
  mark_glow(*colour, fs::exists(glow, ec) ? lab::read_png(glow.string()) : std::nullopt);
  *rgba = std::move(colour->rgba);
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

}  // namespace ge::classicenhance
