// The live map in the window (watch.hpp).
//
// ge:summary The live map in the window: the plate's thread, M and Space, the two layouts
// ge:keywords live-map watch demo_map plate-thread follow-next
// ge:skill replay ui
#include "shooters_edge/livemap/watch.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace shooters_edge::livemap {

namespace cl = shooters_edge::client;
namespace ui = ge::ui;

namespace {

bool tapped(std::span<bool> pressed, int k) {
  return k >= 0 && size_t(k) < pressed.size() && pressed[size_t(k)];
}

}  // namespace

int watch_keys(Watch& w, std::span<bool> pressed, const cl::DemoState& ds, bool tv, bool allowed) {
  if (!allowed) return -1;
  if (tapped(pressed, kKeyM)) {
    w.settings.mode = (w.settings.mode + 1) % 3;
    pressed[size_t(kKeyM)] = false;
  }
  if ((w.settings.mode != 0 || tv) && tapped(pressed, kKeySpaceBar)) {
    pressed[size_t(kKeySpaceBar)] = false;
    return next_followed(ds, ds.player, 1);
  }
  return -1;
}

void watch_level(Watch& w, const cl::SearchPath& files, std::span<const uint8_t> bsp,
                 const cl::DemoState& ds) {
  if (w.building.valid() &&
      w.building.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    std::shared_ptr<const Plate> plate = w.building.get();
    if (plate && w.building_for == w.level) {
      live_map_take(w.map, files, w.palette, std::move(plate));
    } else if (!plate) {
      w.error = "the live map of " + ds.map + " could not be drawn";
    }
  }
  const std::string level = ds.map + "#" + std::to_string(ds.levels);
  if (w.settings.mode == 0 || ds.map.empty() || bsp.empty()) return;
  if (level == w.level && (w.map.plate || w.building.valid() || !w.error.empty())) return;
  if (level != w.level) {
    w.level = level;
    w.error.clear();
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::game_palette(files, store);
    w.palette.assign(pal.begin(), pal.end());
    live_map_take(w.map, files, w.palette, nullptr);
  }
  if (w.building.valid()) return;  // the last level's still running: asked again when it ends
  w.building_for = level;
  std::vector<uint8_t> bytes(bsp.begin(), bsp.end());
  std::vector<uint8_t> pal = w.palette;
  w.building = std::async(std::launch::async,
                          [bytes = std::move(bytes), pal = std::move(pal), name = ds.map]() {
                            auto plate = std::make_shared<Plate>();
                            std::string error;
                            if (!build_plate(bytes, name, pal, kPlatePx, plate.get(), &error))
                              return std::shared_ptr<const Plate>{};
                            return std::shared_ptr<const Plate>(std::move(plate));
                          });
}

bool draw_watch(Watch& w, const cl::DemoState& ds, ui::DrawList& overlay, int width, int height,
                float hud_px, double now_s) {
  if (w.settings.mode == 0 || !w.map.plate) return false;
  LiveView v;
  v.follow = ds.player;
  v.figures = w.settings.figures;
  v.now_s = now_s;
  const auto fw = float(width), fh = float(height);
  if (w.settings.mode == 1) {
    v.area = {0, 0, fw, std::max(fh * 0.5f, fh - hud_px)};
    v.fill = true;
    v.names = w.settings.names != 0;
    v.min_figure_px = std::max(18.0f, fh * 0.035f);
  } else {
    const float side = fh * std::clamp(w.settings.scale, 0.1f, 0.9f);
    const ui::Rect fit = live_map_rect(*w.map.plate, {0, 0, side * 2, side});
    const float margin = std::max(8.0f, fh * 0.015f);
    v.area = {fw - fit.w - margin, margin, fit.w, fit.h};
    v.min_figure_px = std::max(9.0f, side * 0.05f);
  }
  draw_live_map(w.map, ds, v, overlay);
  return true;
}

bool watch_frame(Watch& w, const cl::SearchPath& files, std::span<const uint8_t> bsp,
                 const cl::DemoState& ds, ui::DrawList& overlay, int width, int height,
                 float hud_px) {
  watch_level(w, files, bsp, ds);
  return draw_watch(w, ds, overlay, width, height, hud_px, ds.time);
}

}  // namespace shooters_edge::livemap
