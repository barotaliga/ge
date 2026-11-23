// A classic figure as a picture on the CPU (figure_art.hpp).
//
// ge:summary A classic model read once and one frame of it as a CPU mesh in a player's colours
// ge:keywords figure-art model_art art_mesh translate wear_indexed_skin join_meshes
// ge:skill classic morph
#include "classic/figure_art.hpp"

#include <algorithm>
#include <array>
#include <optional>

#include "assets/image.hpp"
#include "assets/pcx.hpp"
#include "assets/tga.hpp"
#include "morph/overview_mesh.hpp"
#include "morph/read.hpp"

namespace ge::classic {

std::vector<uint8_t> read_skin_picture(const ReadFile& files, std::span<const uint8_t> palette,
                                       const std::string& path, int* w, int* h) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = files(path, store);
  if (bytes.empty()) return {};
  if (path.ends_with(".tga") || path.ends_with(".png")) {
    std::string why;
    std::optional<ge::assets::Image> img = path.ends_with(".tga")
                                               ? ge::assets::decode_tga(bytes, &why)
                                               : ge::assets::decode_png(bytes);
    if (!img) return {};
    *w = img->width;
    *h = img->height;
    return std::move(img->rgba);
  }
  ge::assets::IndexedImage pic;
  std::string err;
  if (!ge::assets::decode_pcx(bytes, path, &pic, &err)) return {};
  *w = pic.width;
  *h = pic.height;
  return ge::assets::pcx_rgba(pic, pic.palette.empty() ? palette : std::span<const uint8_t>{});
}

bool read_model_art(const ReadFile& files, std::span<const uint8_t> palette,
                    const std::string& path, ModelArt* out, const std::string& skin_path) {
  *out = ModelArt{};
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = files(path, store);
  if (bytes.empty()) return false;
  std::string err;
  if (!ge::morph::read_model(bytes, path, &out->model, &err) || out->model.frames.empty())
    return false;
  out->palette.assign(palette.begin(), palette.end());
  if (!skin_path.empty()) {  // the look worn: the model's own skins set aside
    out->picture = read_skin_picture(files, palette, skin_path, &out->pw, &out->ph);
    if (!out->picture.empty()) out->model.skins.clear();
  }
  if (out->model.skins.empty() && out->picture.empty() && !out->model.skin_paths.empty())
    out->picture =
        read_skin_picture(files, palette, out->model.skin_paths.front(), &out->pw, &out->ph);
  out->ok = true;
  return true;
}

int translate(int i, int top, int bottom) {
  const auto row = [](int colour, int j) {
    colour = std::clamp(colour, 0, 13);
    return colour < 8 ? colour * 16 + j : colour * 16 + 15 - j;
  };
  if (top >= 0 && i >= 16 && i < 32) return row(top, i - 16);
  if (bottom >= 0 && i >= 96 && i < 112) return row(bottom, i - 96);
  return i;
}

int frame_index(const ModelArt& a, int group, double time_s) {
  return ge::morph::frame_at(a.model, group, time_s);
}

ge::overview::Mesh art_mesh(const ModelArt& a, int frame, int skin, int top, int bottom) {
  if (!a.ok) return {};
  std::array<uint8_t, 256> remap{};
  for (int i = 0; i < 256; ++i) remap[size_t(i)] = uint8_t(translate(i, top, bottom));
  ge::morph::SkinPaint paint;
  paint.palette = a.palette;
  paint.remap = remap;
  paint.picture = a.picture;
  paint.pw = a.pw;
  paint.ph = a.ph;
  return ge::morph::overview_mesh(a.model, frame, skin, paint);
}

bool wear_indexed_skin(ModelArt& a, std::span<const uint8_t> indices, int w, int h) {
  if (w <= 0 || h <= 0 || w > kSkinFieldWidth || h > kSkinFieldHeight ||
      indices.size() < size_t(w) * size_t(h) || a.model.skins.empty() || a.model.skin_width <= 0 ||
      a.model.skin_height <= 0)
    return false;
  const int sw = a.model.skin_width, sh = a.model.skin_height;
  ge::morph::Skin worn;
  worn.width = sw;
  worn.height = sh;
  worn.indices.assign(size_t(sw) * size_t(sh), 0);
  for (int y = 0; y < sh; ++y)
    for (int x = 0; x < sw; ++x) {
      // The field's top-left 296 x 194 over the skin's whole size, nearest texel.
      const int fx = x * kSkinWornWidth / sw, fy = y * kSkinWornHeight / sh;
      if (fx < w && fy < h)
        worn.indices[size_t(y) * size_t(sw) + size_t(x)] =
            indices[size_t(fy) * size_t(w) + size_t(fx)];
    }
  for (ge::morph::Skin& s : a.model.skins) {
    const float interval = s.interval;
    const int group = s.group;
    s = worn;
    s.interval = interval;
    s.group = group;
  }
  return true;
}

void join_meshes(ge::overview::Mesh& into, const ge::overview::Mesh& add) {
  if (add.idx.empty()) return;
  const auto base = uint32_t(into.pos.size() / 3);
  const int dy = into.th;  // `add`'s texture goes under `into`'s
  into.pos.insert(into.pos.end(), add.pos.begin(), add.pos.end());
  into.st.insert(into.st.end(), add.st.begin(), add.st.end());
  for (size_t i = 0; i + 4 <= add.rect.size(); i += 4) {
    into.rect.push_back(add.rect[i]);
    into.rect.push_back(add.rect[i + 1] + float(dy));
    into.rect.push_back(add.rect[i + 2]);
    into.rect.push_back(add.rect[i + 3]);
  }
  for (const uint32_t k : add.idx) into.idx.push_back(base + k);
  const int tw = std::max(into.tw, add.tw), th = into.th + add.th;
  std::vector<uint8_t> tex(size_t(tw) * size_t(th) * 4, 0);
  for (int y = 0; y < into.th; ++y)
    std::copy_n(into.tex.begin() + std::ptrdiff_t(size_t(y) * size_t(into.tw) * 4),
                size_t(into.tw) * 4, tex.begin() + std::ptrdiff_t(size_t(y) * size_t(tw) * 4));
  for (int y = 0; y < add.th; ++y)
    std::copy_n(add.tex.begin() + std::ptrdiff_t(size_t(y) * size_t(add.tw) * 4),
                size_t(add.tw) * 4, tex.begin() + std::ptrdiff_t(size_t(y + dy) * size_t(tw) * 4));
  into.tex = std::move(tex);
  into.tw = tw;
  into.th = th;
}

}  // namespace ge::classic
