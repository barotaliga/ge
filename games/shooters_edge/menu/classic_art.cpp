// The classic menu's pictures read from the player's pack and coloured by its palette, and the
// stand-in set painted in code for the labs and the tests (classic_art.hpp).
//
// ge:summary Reads the classic menu's pictures off the search path; paints the fixture set
// ge:keywords menu classic pictures lmp wad conchars palette decode fixture paint quantize
// ge:skill ui vfs
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/classic_art.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"
#include "vfs/wad.hpp"

namespace shooters_edge::menu {

namespace {

namespace ui = ge::ui;
namespace vfs = ge::vfs;

constexpr std::string_view kNames[] = {
    "qplaque",  "ttl_main", "mainmenu", "menudot1", "menudot2", "menudot3", "menudot4", "menudot5",
    "menudot6", "ttl_sgl",  "sp_menu",  "p_multi",  "mp_menu",  "p_option", "help0",    "help1",
    "help2",    "help3",    "help4",    "help5",    "box_tl",   "box_tm",   "box_tr",   "box_ml",
    "box_mm",   "box_mr",   "box_bl",   "box_bm",   "box_br",   "complete", "inter",    "ranking"};

// The big figures the level's end is drawn in, pictures of the glyph sheet's wad.
constexpr std::string_view kWadNames[] = {"num_0",     "num_1",     "num_2",    "num_3", "num_4",
                                          "num_5",     "num_6",     "num_7",    "num_8", "num_9",
                                          "num_minus", "num_colon", "num_slash"};
// The wad's other pictures: the status bar's strip the scores key shows (320 x 24).
constexpr std::string_view kWadStrips[] = {"scorebar"};

ui::Image colour(const vfs::IndexedPicture& p, std::span<const uint8_t> palette, int clear) {
  ui::Image img;
  img.width = p.width;
  img.height = p.height;
  img.rgba.resize(static_cast<size_t>(p.width) * static_cast<size_t>(p.height) * 4);
  for (size_t i = 0; i < p.indices.size(); i++) {
    const uint8_t k = p.indices[i];
    uint8_t* px = &img.rgba[i * 4];
    const size_t at = size_t{k} * 3;
    px[0] = palette[at];
    px[1] = palette[at + 1];
    px[2] = palette[at + 2];
    px[3] = k == clear ? 0 : 255;
  }
  return img;
}

// --- the fixture: a warm ramp palette, pictures painted with the UI's font and quantized ---

constexpr int kClear = 255;

std::vector<uint8_t> fixture_palette() {
  std::vector<uint8_t> p(768);
  for (int i = 0; i < 256; i++) {
    const double v = std::min(1.0, i / 223.0);
    p[static_cast<size_t>(i) * 3] = static_cast<uint8_t>(std::lround(255 * v));
    p[static_cast<size_t>(i) * 3 + 1] = static_cast<uint8_t>(std::lround(255 * v * v * 0.86));
    p[static_cast<size_t>(i) * 3 + 2] = static_cast<uint8_t>(std::lround(255 * v * v * v * 0.55));
  }
  return p;
}

// A drawn picture as palette indices: see-through below half cover, else the ramp's step for its
// brightness.
vfs::IndexedPicture quantize(const ui::Image& img, int clear) {
  vfs::IndexedPicture p{img.width, img.height, {}};
  p.indices.resize(static_cast<size_t>(img.width) * static_cast<size_t>(img.height));
  for (size_t i = 0; i < p.indices.size(); i++) {
    const uint8_t* px = &img.rgba[i * 4];
    if (px[3] < 128) {
      p.indices[i] = static_cast<uint8_t>(clear);
      continue;
    }
    const int k = 1 + (px[0] * 222) / 255;  // the red channel carries the ramp
    p.indices[i] = static_cast<uint8_t>(std::clamp(k, 1, 223));
  }
  return p;
}

ui::Image canvas(int w, int h, const std::function<void(ui::DrawList&)>& paint) {
  ui::DrawList dl;
  dl.begin(w, h);
  paint(dl);
  ui::Image img = ui::make_image(w, h, {0, 0, 0, 0});
  ui::render(dl, img);
  return img;
}

const ui::Color kGold{0.92f, 0.78f, 0.5f, 1};
const ui::Color kBronze{0.45f, 0.33f, 0.2f, 1};
const ui::Color kDark{0.16f, 0.12f, 0.08f, 1};

// A plate with bevelled edges and words centred on it.
ui::Image plate(int w, int h, std::string_view words, float size) {
  return canvas(w, h, [&](ui::DrawList& dl) {
    dl.rect({0, 0, float(w), float(h)}, kBronze, 2);
    dl.rect({2, 2, float(w) - 4, float(h) - 4}, kDark, 1);
    const ui::Font& f = ui::ui_font(ui::Weight::semibold);
    dl.text(f, size, float(w) / 2, float(h) / 2 + size * 0.36f, words, kGold, ui::Align::center, 1);
  });
}

// A strip of rows 20 px high, each its words in the pack's carved style.
ui::Image strip(std::span<const std::string_view> rows) {
  const int h = 20 * static_cast<int>(rows.size());
  return canvas(240, h, [&](ui::DrawList& dl) {
    const ui::Font& f = ui::ui_font(ui::Weight::semibold);
    for (size_t i = 0; i < rows.size(); i++) {
      const float y = 20.0f * static_cast<float>(i);
      dl.text(f, 17, 1, y + 17, rows[i], kDark, ui::Align::left, 1.5f);
      dl.text(f, 17, 0, y + 16, rows[i], kGold, ui::Align::left, 1.5f);
    }
  });
}

// The cursor: a disc with a notch, turned a sixth further each frame.
ui::Image dot(int frame) {
  return canvas(16, 20, [&](ui::DrawList& dl) {
    const float a = static_cast<float>(frame) * 1.0471976f;
    dl.arc({8, 10}, 4.5f, 5, a + 0.6f, a + 5.7f, ui::solid(kGold), false, 1);
    dl.arc({8, 10}, 4.5f, 5, a - 0.6f, a + 0.6f, ui::solid(kBronze), false, 1);
  });
}

ui::Image help_page(int n) {
  return canvas(320, 200, [&](ui::DrawList& dl) {
    dl.rect({0, 0, 320, 200}, kDark);
    dl.rect({6, 6, 308, 188}, {0.24f, 0.18f, 0.12f, 1}, 4);
    const ui::Font& f = ui::ui_font(ui::Weight::semibold);
    dl.text(f, 22, 160, 44, "HELP", kGold, ui::Align::center, 2);
    dl.text(f, 13, 160, 110, "PAGE " + std::to_string(n + 1) + " OF 6", kGold, ui::Align::center);
    dl.text(f, 10, 160, 180, "THE PACK'S OWN PAGE IS DRAWN HERE", kBronze, ui::Align::center);
  });
}

// The 128 x 128 glyph sheet: printable ASCII in its cells, the same again from 128 in the darker
// set, the slider's pieces at 128 to 131, and the blinking cursor at 12 and 13.
vfs::IndexedPicture glyph_sheet() {
  ui::Image img = canvas(128, 128, [](ui::DrawList& dl) {
    const ui::Font& f = ui::ui_font(ui::Weight::semibold);
    for (int set = 0; set < 2; set++) {
      for (int c = 33; c < 127; c++) {
        const int k = c + set * 128;
        const float x = static_cast<float>((k & 15) * 8), y = static_cast<float>((k >> 4) * 8);
        const char s[2] = {static_cast<char>(c), 0};
        dl.text(f, 8, x + 4, y + 7, s, set == 0 ? ui::Color{1, 1, 1, 1} : kGold, ui::Align::center);
      }
    }
    const auto cell = [](int k) {
      return ui::Rect{static_cast<float>((k & 15) * 8), static_cast<float>((k >> 4) * 8), 8, 8};
    };
    for (int k : {128, 129, 130}) {
      const ui::Rect r = cell(k);
      dl.rect({r.x, r.y + 3, 8, 2}, kBronze);
    }
    // 16 and 17 the gold brackets round the player's own frags, 29 30 31 a rule's left end,
    // middle and right end (the scoreboard's header underline).
    const ui::Rect lb = cell(16), rb = cell(17);
    dl.rect({lb.x + 1, lb.y, 2, 8}, kGold);
    dl.rect({lb.x + 1, lb.y, 5, 2}, kGold);
    dl.rect({lb.x + 1, lb.y + 6, 5, 2}, kGold);
    dl.rect({rb.x + 5, rb.y, 2, 8}, kGold);
    dl.rect({rb.x + 2, rb.y, 5, 2}, kGold);
    dl.rect({rb.x + 2, rb.y + 6, 5, 2}, kGold);
    for (int k : {29, 30, 31}) {
      const ui::Rect r = cell(k);
      dl.rect({r.x + (k == 29 ? 1.0f : 0.0f), r.y + 3, k == 30 ? 8.0f : 7.0f, 2}, kGold);
    }
    const ui::Rect h = cell(131);
    dl.rect({h.x + 2, h.y + 1, 4, 6}, kGold, 1);
    // 12 is the cursor's blank half, 13 its arrow.
    const ui::Rect r = cell(13);
    const ui::Vec2 pts[3] = {{r.x + 1, r.y + 1}, {r.x + 7, r.y + 4}, {r.x + 1, r.y + 7}};
    dl.polygon(pts, ui::solid(kGold));
  });
  return quantize(img, 0);  // index 0 is the sheet's see-through
}

std::vector<uint8_t> lmp(const ui::Image& img) { return vfs::write_picture(quantize(img, kClear)); }

}  // namespace

const ui::Image* ClassicArt::find(std::string_view name) const {
  const auto it = pictures.find(name);
  return it == pictures.end() ? nullptr : &it->second;
}

ui::Color ClassicArt::colour(int index) const {
  const size_t at = static_cast<size_t>(std::clamp(index, 0, 255)) * 3;
  return {static_cast<float>(palette[at]) / 255, static_cast<float>(palette[at + 1]) / 255,
          static_cast<float>(palette[at + 2]) / 255, 1};
}

std::span<const std::string_view> classic_picture_names() { return kNames; }

bool load_classic_art(const FindFile& find, ClassicArt* out, std::string* error,
                      const PictureUpgrade& upgrade) {
  *out = {};
  std::vector<uint8_t> pal_store, wad_store, store;
  const std::span<const uint8_t> palette = find("gfx/palette.lmp", pal_store);
  if (palette.size() != 768) {
    *error = "the packs have no gfx/palette.lmp of 768 bytes";
    return false;
  }
  const std::span<const uint8_t> wad_bytes = find("gfx.wad", wad_store);
  vfs::Wad wad;
  std::string why;
  if (wad_bytes.empty() || !vfs::read_wad(wad_bytes, &wad, &why)) {
    *error = "the packs have no readable gfx.wad" + (why.empty() ? "" : ": " + why);
    return false;
  }
  const vfs::WadEntry* sheet = vfs::wad_find(wad, "conchars");
  vfs::IndexedPicture glyphs;
  if (sheet == nullptr ||
      !vfs::read_raw_picture(vfs::wad_lump(wad_bytes, *sheet), 128, 128, &glyphs, &why)) {
    *error = "gfx.wad has no 128 x 128 glyph sheet `conchars`";
    return false;
  }
  // Each picture as the pack has it, or its enhanced copy at the glyph sheet's density.
  uint32_t density = 0;
  const auto picture = [&](std::string_view cache_name, const vfs::IndexedPicture& p, int clear) {
    ui::Image img = colour(p, palette, clear);
    ui::Image big;
    const uint32_t k = upgrade ? upgrade(cache_name, p.width, p.height, &big) : 0;
    if (density == 0) density = k == 0 ? 1 : k;
    if (k == density && k > 1) return big;
    return ui::scaled_nearest(img, static_cast<int>(density));
  };
  out->glyphs = picture("conchars", glyphs, 0);
  out->density = static_cast<float>(density);
  std::copy(palette.begin(), palette.end(), out->palette.begin());
  for (std::string_view name : kNames) {
    const std::string path = "gfx/" + std::string(name) + ".lmp";
    const std::span<const uint8_t> bytes = find(path, store);
    vfs::IndexedPicture p;
    if (bytes.empty() || !vfs::read_picture(bytes, &p, &why)) continue;
    out->pictures.emplace(std::string(name), picture("gfx/" + std::string(name), p, kClear));
  }
  for (const auto names : {std::span<const std::string_view>(kWadNames),
                           std::span<const std::string_view>(kWadStrips)}) {
    for (std::string_view name : names) {
      const vfs::WadEntry* e = vfs::wad_find(wad, name);
      vfs::IndexedPicture p;
      if (e == nullptr || !vfs::read_picture(vfs::wad_lump(wad_bytes, *e), &p, &why)) continue;
      out->pictures.emplace(std::string(name), picture(name, p, kClear));
    }
  }
  if (out->find("mainmenu") == nullptr) {
    *error = "the packs have no gfx/mainmenu.lmp";
    *out = {};
    return false;
  }
  return true;
}

std::map<std::string, std::vector<uint8_t>, std::less<>> fixture_files() {
  std::map<std::string, std::vector<uint8_t>, std::less<>> files;
  files["gfx/palette.lmp"] = fixture_palette();
  std::vector<uint8_t> wad;
  std::string error;
  std::vector<vfs::WadLump> lumps = {{"conchars", vfs::kRawLump, glyph_sheet().indices}};
  // The big figures: 24 px tall, the colon and the slash 16 px wide.
  for (std::string_view name : kWadNames) {
    const std::string_view what = name.substr(4);
    const bool narrow = what == "colon" || what == "slash";
    const std::string word = what == "minus"   ? "-"
                             : what == "colon" ? ":"
                             : what == "slash" ? "/"
                                               : std::string(what);
    lumps.push_back({std::string(name), vfs::kPictureLump,
                     lmp(canvas(narrow ? 16 : 24, 24, [&](ui::DrawList& dl) {
                       const ui::Font& f = ui::ui_font(ui::Weight::semibold);
                       dl.text(f, 22, narrow ? 8.0f : 12.0f, 21, word, kGold, ui::Align::center);
                     }))});
  }
  lumps.push_back({"scorebar", vfs::kPictureLump, lmp(canvas(320, 24, [](ui::DrawList& dl) {
                     dl.rect({0, 0, 320, 24}, kDark);
                     dl.rect({0, 0, 320, 1}, kBronze);
                     dl.rect({0, 23, 320, 1}, kBronze);
                   }))});
  vfs::write_wad(lumps, &wad, &error);
  files["gfx.wad"] = std::move(wad);
  const auto put = [&](std::string_view name, const ui::Image& img) {
    files["gfx/" + std::string(name) + ".lmp"] = lmp(img);
  };
  put("qplaque", canvas(32, 144, [](ui::DrawList& dl) {
        dl.rect({0, 0, 32, 144}, kBronze, 3);
        dl.rect({3, 3, 26, 138}, kDark, 2);
        for (int i = 0; i < 4; i++)
          dl.rect({9, 18.0f + 30 * static_cast<float>(i), 14, 18}, kGold, 3);
      }));
  put("ttl_main", plate(96, 24, "MAIN MENU", 13));
  put("complete", plate(192, 24, "COMPLETE", 15));
  put("inter", canvas(160, 144, [](ui::DrawList& dl) {
        const ui::Font& f = ui::ui_font(ui::Weight::semibold);
        constexpr std::string_view kRows[] = {"TIME", "SECRETS", "KILLS"};
        for (int i = 0; i < 3; i++)
          dl.text(f, 18, 4, 30.0f + 40 * static_cast<float>(i), kRows[i], kGold, ui::Align::left,
                  1);
      }));
  put("ranking", plate(144, 24, "RANKINGS", 15));
  put("ttl_sgl", plate(128, 24, "SINGLE PLAYER", 13));
  put("p_multi", plate(120, 24, "MULTIPLAYER", 13));
  put("p_option", plate(96, 24, "OPTIONS", 13));
  constexpr std::string_view kMain[] = {"SINGLE PLAYER", "MULTIPLAYER", "OPTIONS", "HELP", "QUIT"};
  constexpr std::string_view kSingle[] = {"NEW GAME", "LOAD", "SAVE"};
  constexpr std::string_view kMulti[] = {"JOIN A GAME", "NEW GAME", "SETUP"};
  put("mainmenu", strip(kMain));
  put("sp_menu", strip(kSingle));
  put("mp_menu", strip(kMulti));
  for (int i = 0; i < 6; i++) put("menudot" + std::to_string(i + 1), dot(i));
  for (int i = 0; i < 6; i++) put("help" + std::to_string(i), help_page(i));
  const auto piece = [](int w, bool top, bool bottom, bool left, bool right) {
    return canvas(w, 8, [=](ui::DrawList& dl) {
      dl.rect({0, 0, float(w), 8}, kDark);
      if (top) dl.rect({0, 0, float(w), 2}, kBronze);
      if (bottom) dl.rect({0, 6, float(w), 2}, kBronze);
      if (left) dl.rect({0, 0, 2, 8}, kBronze);
      if (right) dl.rect({float(w) - 2, 0, 2, 8}, kBronze);
    });
  };
  put("box_tl", piece(8, true, false, true, false));
  put("box_tm", piece(16, true, false, false, false));
  put("box_tr", piece(8, true, false, false, true));
  put("box_ml", piece(8, false, false, true, false));
  put("box_mm", piece(16, false, false, false, false));
  put("box_mr", piece(8, false, false, false, true));
  put("box_bl", piece(8, false, true, true, false));
  put("box_bm", piece(16, false, true, false, false));
  put("box_br", piece(8, false, true, false, true));
  return files;
}

}  // namespace shooters_edge::menu
