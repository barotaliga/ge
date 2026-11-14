// The sequel's status bar in the window (sequel_bar.hpp).
//
// ge:summary The sequel's status bar drawn in the window from its program and the game's pictures
// ge:keywords sequel status-bar draw_sequel_bar run_sequel_bar
// ge:skill ui shooter
// ge:test tests/shooters_edge_sequel_screen_test.cpp
#include "shooters_edge/client/sequel_bar.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <utility>

#include "assets/pcx.hpp"

namespace shooters_edge::client {

namespace {

constexpr int kClear = 255;  // the pictures' see-through colour

const ge::ui::Image* picture(SequelBar& bar, const std::string& path) {
  auto it = bar.pictures.find(path);
  if (it == bar.pictures.end()) {
    ge::ui::Image out;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes =
        bar.files != nullptr ? find_file(*bar.files, path, store) : std::span<const uint8_t>();
    ge::assets::IndexedImage img;
    std::string why;
    if (!bytes.empty() && ge::assets::decode_pcx(bytes, path, &img, &why) && !img.palette.empty()) {
      out.width = img.width;
      out.height = img.height;
      out.rgba = ge::assets::pcx_rgba(img);
      for (size_t i = 0; i < img.indices.size(); i++)
        if (img.indices[i] == kClear) out.rgba[i * 4 + 3] = 0;
    }
    it = bar.pictures.emplace(path, std::move(out)).first;
  }
  return it->second.empty() ? nullptr : &it->second;
}

struct Strings {
  const server::RulesWorld& world;
  mutable std::string scratch;
};

}  // namespace

std::string_view sequel_world_string(const server::RulesWorld& w, int index, std::string& scratch) {
  const auto in = [](const std::vector<std::string>& list, int n) -> std::string_view {
    return n > 0 && n < static_cast<int>(list.size()) ? std::string_view(list[size_t(n)])
                                                      : std::string_view();
  };
  if (index >= hud::kLayoutImages && index < hud::kLayoutImages + 256)
    return in(w.images, index - hud::kLayoutImages);
  if (index >= 1056 && index < 1056 + 256) return in(w.item_names, index - 1056);
  if (index >= hud::kLayoutSkins && index < hud::kLayoutSkins + ge::shooter::kMaxClients) {
    const server::WorldClient& c = w.clients[index - hud::kLayoutSkins];
    if (!c.active) return {};
    scratch = c.name + "\\male/grunt";
    return scratch;
  }
  return {};
}

int sequel_bar_scale(int height) { return std::max(1, height / 400); }

void run_sequel_bar(SequelBar& bar, const server::RulesWorld& world, int slot, int64_t frame,
                    int width, int height) {
  bar.ops.clear();
  if (world.status_bar.empty() || slot < 0 || slot >= ge::shooter::kMaxClients) return;
  const int scale = sequel_bar_scale(height);
  Strings strings{world, {}};
  hud::LayoutInput in;
  in.program = world.status_bar;
  in.stats = std::span<const int16_t>(world.clients[slot].numbers.stats, hud::kLayoutStats);
  in.width = width / scale;
  in.height = height / scale;
  in.frame = frame;
  in.self = slot;
  in.ctx = &strings;
  in.string = [](const void* ctx, int index) {
    const auto& s = *static_cast<const Strings*>(ctx);
    return sequel_world_string(s.world, index, s.scratch);
  };
  hud::run_layout(in, bar.ops);  // its text copied out of the strings as they are read
  const server::WorldClient& me = world.clients[slot];
  if ((in.stats[13] & 1) == 0 || me.layout.empty()) return;
  std::vector<hud::LayoutOp> over;  // the screen the rules sent him, over the bar
  in.program = me.layout;
  hud::run_layout(in, over);
  for (hud::LayoutOp& op : over) bar.ops.push_back(std::move(op));
}

void add_sequel_screen(SequelBar& bar, int crosshair, std::string_view centre, int width,
                       int height) {
  const int scale = sequel_bar_scale(height);
  const int w = width / scale, h = height / scale;
  if (crosshair >= 1 && crosshair <= 3) {
    hud::LayoutOp op;
    op.name = "ch" + std::to_string(crosshair);
    if (const ge::ui::Image* img = picture(bar, hud::layout_picture_path(op.name))) {
      op.x = (w - img->width) / 2;
      op.y = (h - img->height) / 2;
      bar.ops.insert(bar.ops.begin(), std::move(op));  // under the bar
    }
  }
  if (centre.empty()) return;
  std::vector<std::string_view> lines;
  for (size_t at = 0; at <= centre.size();) {
    const size_t end = std::min(centre.find('\n', at), centre.size());
    lines.push_back(centre.substr(at, end - at));
    at = end + 1;
  }
  int y = lines.size() <= 4 ? h * 35 / 100 : 48;
  for (const std::string_view line : lines) {
    hud::LayoutOp op;
    op.kind = hud::LayoutOp::Kind::text;
    op.text = std::string(line.substr(0, std::min<size_t>(line.size(), 40)));
    op.x = (w - 8 * static_cast<int>(op.text.size())) / 2;
    op.y = y;
    bar.ops.push_back(std::move(op));
    y += 8;
  }
}

bool draw_sequel_bar(ge::ui::DrawList& dl, SequelBar& bar, int scale) {
  const auto f = static_cast<float>(scale);
  bool drew = false;
  const auto box = [&](const ge::ui::Image* img, float x, float y, float w, float h,
                       ge::ui::Rect uv) {
    ge::ui::Box b;
    b.rect = {x * f, y * f, w * f, h * f};
    b.image = img;
    b.uv = uv;
    b.nearest = true;
    b.feather = 0;
    dl.box(b);
    drew = true;
  };
  const ge::ui::Image* glyphs = nullptr;
  for (const hud::LayoutOp& op : bar.ops) {
    if (op.kind == hud::LayoutOp::Kind::picture) {
      const ge::ui::Image* img = picture(bar, hud::layout_picture_path(op.name));
      if (img == nullptr && !op.fallback.empty())
        img = picture(bar, hud::layout_picture_path(op.fallback));
      if (img == nullptr) continue;
      box(img, static_cast<float>(op.x), static_cast<float>(op.y), static_cast<float>(img->width),
          static_cast<float>(img->height),
          {0, 0, static_cast<float>(img->width), static_cast<float>(img->height)});
      continue;
    }
    if (glyphs == nullptr) glyphs = picture(bar, "pics/conchars.pcx");
    if (glyphs == nullptr) return drew;
    const float cell = static_cast<float>(glyphs->width) / 16;
    for (size_t i = 0; i < op.text.size(); i++) {
      auto code = static_cast<uint8_t>(op.text[i]);
      if (op.alt) code = static_cast<uint8_t>(code | 0x80);
      if ((code & 0x7F) == ' ') continue;
      const float x = static_cast<float>(op.x) + 8 * static_cast<float>(i);
      if (op.y <= -8) continue;
      box(glyphs, x, static_cast<float>(op.y), 8, 8,
          {static_cast<float>(code & 15) * cell, static_cast<float>(code >> 4) * cell, cell, cell});
    }
  }
  return drew;
}

}  // namespace shooters_edge::client
