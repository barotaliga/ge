// The live map in the window (watch.hpp).
//
// ge:summary The live map in the window: the plate's thread, M and Space, the two layouts
// ge:keywords live-map watch demo_map plate-thread follow-next
// ge:skill replay ui
#include "shooters_edge/livemap/watch.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "brush/map.hpp"
#include "core/math.hpp"

namespace shooters_edge::livemap {

namespace cl = shooters_edge::client;
namespace ui = ge::ui;
namespace ov = ge::overview;

namespace {

// The eye over the floor a player stands on, units (his view height over his origin, and his
// origin over his feet, as both games have them).
constexpr double kEyeOverFloorUnits = 22 + 24;

bool tapped(std::span<bool> pressed, int k) {
  return k >= 0 && size_t(k) < pressed.size() && pressed[size_t(k)];
}

// The whole map's area: the screen over the HUD's band.
ui::Rect whole_area(int width, int height, float hud_px) {
  const auto fw = float(width), fh = float(height);
  return {0, 0, fw, std::max(fh * 0.5f, fh - hud_px)};
}

// M: from the map down to the eye, from the eye (or the corner) up to the map; a fly running
// turns back. Without a plate yet, the map just shows or goes.
void m_pressed(Watch& w) {
  ov::Fly& f = w.fly;
  if (f.way == ov::FlyWay::down) return ov::fly_start(f, ov::FlyWay::up);
  if (f.way == ov::FlyWay::up) return ov::fly_start(f, ov::FlyWay::down);
  if (w.settings.mode == 1) {
    if (!w.map.plate) {
      w.settings.mode = 0;
      return;
    }
    f.p = 0;
    ov::fly_start(f, ov::FlyWay::down);
    return;
  }
  w.settings.mode = 1;
  f.p = w.map.plate ? 1 : 0;
  if (w.map.plate) ov::fly_start(f, ov::FlyWay::up);
}

}  // namespace

int watch_keys(Watch& w, std::span<bool> pressed, const cl::DemoState& ds, bool tv, bool allowed) {
  if (!allowed) return -1;
  if (tapped(pressed, kKeyM)) {
    m_pressed(w);
    pressed[size_t(kKeyM)] = false;
  }
  if ((w.settings.mode != 0 || tv) && tapped(pressed, kKeySpaceBar)) {
    pressed[size_t(kKeySpaceBar)] = false;
    return next_followed(ds, ds.player, 1);
  }
  return -1;
}

double watch_demo_rate(const Watch& w) { return ov::fly_demo_rate(w.fly, w.fly_times); }

double watch_fly_step(Watch& w, double wall_dt) {
  ov::Fly& f = w.fly;
  const bool diving = f.way == ov::FlyWay::down;
  ov::fly_advance(f, wall_dt, w.fly_times);
  // Quiet through the fly and its catch-up, and for an arrival's length after.
  w.quiet_s = ov::fly_running(f) ? client::kArriveS + 0.3 : std::max(0.0, w.quiet_s - wall_dt);
  if (f.way != ov::FlyWay::none) return watch_demo_rate(w);
  if (diving && f.p >= 1) w.settings.mode = 0;  // landed in the eye: the map is off
  // At rest the fly stands where the setting says: the whole map at the top, else the eye.
  f.p = w.settings.mode == 1 ? 0 : 1;
  return watch_demo_rate(w);
}

client::FxSwitches watch_fx(const Watch& w, client::FxSwitches on) {
  if (w.quiet_s > 0) on.arrival = 0;
  return on;
}

bool watch_fly_view(Watch& w, ge::render::FrameView& view, int width, int height, float hud_px) {
  const ov::Fly& f = w.fly;
  w.fly_place = {};
  if (!w.map.plate || f.p >= 1 || (f.way == ov::FlyWay::none && f.p <= 0) || width <= 0 ||
      height <= 0)
    return false;
  const Plate& pl = *w.map.plate;
  const ov::Frame& fr = pl.frame;
  // The level's box in engine metres (the plate's x, its y north (+z), its z up (+y)).
  const double k = ge::brush::kUnitM;
  const double lo[3] = {fr.lo[0] * k, fr.lo[2] * k, fr.lo[1] * k};
  const double hi[3] = {fr.hi[0] * k, fr.hi[2] * k, fr.hi[1] * k};
  // Where the whole map draws the level's box on the screen.
  const ui::Rect fit = live_map_rect(pl, whole_area(width, height, hud_px));
  const double s = pl.image.width > 0 ? double(fit.w) / double(pl.image.width) : 1;
  const double box[4] = {fit.x + fr.pad * s, fit.y + fr.pad * s,
                         fit.x + (fr.pad + (fr.hi[0] - fr.lo[0]) * fr.scale) * s,
                         fit.y + (fr.pad + (fr.hi[1] - fr.lo[1]) * fr.scale) * s};
  const ov::FlyPose top = ov::top_pose_onto(lo, hi, view.fov_y, width, height, view.shift_y, box);
  ov::FlyPose eye;
  for (int q = 0; q < 3; ++q) eye.eye[q] = view.eye[q];
  eye.heading = view.heading;
  eye.pitch = view.pitch;
  const ov::FlyPose now = ov::fly_pose(f, w.fly_times, top, eye);
  // The whole map zoomed with the eye: the level's plan where that eye sees it, the plane it lies
  // on sliding from a third of the way up the level to the floor under the player, and the
  // figures from their map size down to life size by the time the level takes over.
  const double near = ge::core::smoothstep(0, w.fly_times.blend_to, f.p);
  const double plane =
      ge::core::lerp(lo[1] + (hi[1] - lo[1]) / 3, eye.eye[1] - kEyeOverFloorUnits * k, near);
  double zoom[4];
  ov::fly_plan_box(now, lo, hi, plane, view.fov_y, width, height, view.shift_y, zoom);
  // The plan's box back to the plate's picture (its pad round the plan).
  const double ppx = (zoom[2] - zoom[0]) / std::max(1e-6, (fr.hi[0] - fr.lo[0]) * fr.scale);
  w.fly_place = {float(zoom[0] - fr.pad * ppx), float(zoom[1] - fr.pad * ppx),
                 float(pl.image.width * ppx), float(pl.image.height * ppx)};
  w.fly_figures = float(ge::core::lerp(w.settings.figures, 1, near));
  for (int q = 0; q < 3; ++q) view.eye[q] = now.eye[q];
  view.heading = now.heading;
  view.pitch = now.pitch;
  view.roll *= f.p;  // level from above, the player's own roll as it lands
  view.near_m = std::max(view.near_m, (now.eye[1] - eye.eye[1]) * 0.01);
  return true;
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
  if (ds.map.empty() || bsp.empty()) return;  // made even with the map off: M flies up to it
  if (level == w.level && (w.map.plate || w.building.valid() || !w.error.empty())) return;
  if (level != w.level) {
    w.level = level;
    w.error.clear();
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = recording_palette(files, ds, store);
    w.palette.assign(pal.begin(), pal.end());
    live_map_take(w.map, files, w.palette, nullptr);
  }
  if (w.building.valid()) return;  // the last level's still running: asked again when it ends
  w.building_for = level;
  std::vector<uint8_t> bytes(bsp.begin(), bsp.end());
  std::vector<uint8_t> pal = w.palette;
  w.building = std::async(std::launch::async, [bytes = std::move(bytes), pal = std::move(pal),
                                               name = ds.map, files = &files]() {
    auto plate = std::make_shared<Plate>();
    std::string error;
    if (!build_plate(bytes, name, pal, kPlatePx, plate.get(), &error, files))
      return std::shared_ptr<const Plate>{};
    return std::shared_ptr<const Plate>(std::move(plate));
  });
}

bool draw_watch(Watch& w, const cl::DemoState& ds, ui::DrawList& overlay, int width, int height,
                float hud_px, double now_s) {
  if (w.settings.mode == 0 || !w.map.plate) return false;
  // During a fly the whole map fades as the level from above takes its place.
  const double share = w.settings.mode == 1 ? ov::fly_map_share(w.fly, w.fly_times) : 1;
  if (share <= 0) return false;
  const size_t from = overlay.size();
  LiveView v;
  v.follow = ds.player;
  v.figures = w.settings.figures;
  v.now_s = now_s;
  const auto fw = float(width), fh = float(height);
  if (w.settings.mode == 1) {
    // The void under the HUD's band too, where a narrow status bar leaves the sides open.
    overlay.rect({0, 0, fw, fh}, w.map.plate->void_colour);
    v.area = whole_area(width, height, hud_px);
    v.fill = true;
    v.names = w.settings.names != 0;
    v.min_figure_px = std::max(18.0f, fh * 0.035f);
    if (w.fly_place.w > 0) {  // the fly zooming the map onto the player
      v.place = w.fly_place;
      v.figures = w.fly_figures;
    }
  } else {
    const float side = fh * std::clamp(w.settings.scale, 0.1f, 0.9f);
    const ui::Rect fit = live_map_rect(*w.map.plate, {0, 0, side * 2, side});
    const float margin = std::max(8.0f, fh * 0.015f);
    v.area = {fw - fit.w - margin, margin, fit.w, fit.h};
    v.min_figure_px = std::max(9.0f, side * 0.05f);
  }
  draw_live_map(w.map, ds, v, overlay);
  if (share < 1) overlay.fade(from, float(share));
  return true;
}

bool watch_frame(Watch& w, const cl::SearchPath& files, std::span<const uint8_t> bsp,
                 const cl::DemoState& ds, ui::DrawList& overlay, int width, int height,
                 float hud_px) {
  watch_level(w, files, bsp, ds);
  return draw_watch(w, ds, overlay, width, height, hud_px, ds.time);
}

}  // namespace shooters_edge::livemap
