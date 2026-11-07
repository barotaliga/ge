// The bot team's map in the window (bot_view.hpp).
//
// ge:summary The bot team's map in the window: the map from above, areas by belief, players, plan
// ge:keywords bot-view project floor-cells overlay panel feed legend
// ge:skill bots render ui
#include "shooters_edge/client/bot_view.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "shooter/hull.hpp"
#include "shooter/locs.hpp"
#include "shooter/pilot.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_plan.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "ui/font.hpp"

namespace shooters_edge::client {

namespace {

namespace bt = shooters_edge::bots;
namespace sh = ge::shooter;
using ge::ui::Color;
using ge::ui::Vec2;

constexpr double kFov = 0.6;  // rad: a narrow lens, so the floors stack with little parallax
constexpr int kPanel = 420;   // the panel's width, px
constexpr int kHidMinus = 45, kHidEquals = 46, kHidHome = 74;

const Color kSafe{0.15f, 0.85f, 0.3f, 0.38f};
const Color kLost{0.95f, 0.15f, 0.12f, 0.42f};
const Color kContested{1.0f, 0.55f, 0.05f, 0.5f};
const Color kUnknown{0.5f, 0.5f, 0.5f, 0.22f};
const Color kWater{0.1f, 0.35f, 1.0f, 0.55f};
const Color kSlime{0.35f, 0.7f, 0.1f, 0.6f};
const Color kLava{1.0f, 0.25f, 0.0f, 0.7f};
const Color kInk{0.92f, 0.92f, 0.9f, 1};
const Color kDim{0.62f, 0.62f, 0.6f, 1};
const Color kEnemy{1.0f, 0.2f, 0.15f, 1};
const Color kBots[8] = {{0.3f, 0.78f, 1, 1},  {0.47f, 1, 0.47f, 1}, {1, 0.86f, 0.31f, 1},
                        {0.9f, 0.47f, 1, 1},  {1, 1, 1, 1},         {0.31f, 1, 0.86f, 1},
                        {1, 0.63f, 0.63f, 1}, {0.7f, 0.7f, 1, 1}};

Color area_colour(sn::AreaState s) {
  switch (s) {
    case sn::AreaState::safe:
      return kSafe;
    case sn::AreaState::lost:
      return kLost;
    case sn::AreaState::contested:
      return kContested;
    default:
      return kUnknown;
  }
}

std::string clock_text(double s) {
  const int t = static_cast<int>(std::max(0.0, s));
  char b[16];
  std::snprintf(b, sizeof b, "%d:%02d", t / 60, t % 60);
  return b;
}

const char* weapon_name(int32_t bit) {
  switch (bit) {
    case 1:
      return "sg";
    case 2:
      return "ssg";
    case 4:
      return "ng";
    case 8:
      return "sng";
    case 16:
      return "gl";
    case 32:
      return "rl";
    case 64:
      return "lg";
    case 4096:
      return "axe";
    default:
      return "-";
  }
}

// A map point (units) on the picture, px; false behind the eye.
struct Projector {
  ge::render::CameraMatrices m;
  double eye[3];
  int w = 0, h = 0;
  bool at(sh::Vec3f p, Vec2& out) const {
    const ge::camera::Vec3 e = engine_point(p);
    const float in[4] = {static_cast<float>(e.x - eye[0]), static_cast<float>(e.y - eye[1]),
                         static_cast<float>(e.z - eye[2]), 1};
    float c[4];
    ge::render::transform(m.view_proj, in, c);
    if (c[3] <= 1e-6f) return false;
    out.x = (c[0] / c[3] * 0.5f + 0.5f) * static_cast<float>(w);
    out.y = (0.5f - c[1] / c[3] * 0.5f) * static_cast<float>(h);
    return true;
  }
};

void fit(BotCam& cam, const sn::BotTeam& t) {
  const sh::NavGraph& g = t.nav->nav.graph;
  cam.cx = (g.lo.x + g.hi.x) * 0.5;
  cam.cy = (g.lo.y + g.hi.y) * 0.5;
  cam.span = std::max(static_cast<double>(g.hi.y - g.lo.y), (g.hi.x - g.lo.x) * 0.6) * 1.08 + 64;
  cam.fitted = true;
}

}  // namespace

void bot_cam_input(BotCam& cam, const sn::BotTeam& t, const ge::platform::InputSnapshot& in,
                   bool keys, double dt, int h, float px_per_unit) {
  if (!t.nav) return;
  if (!cam.fitted || (keys && in.pressed[kHidHome])) fit(cam, t);
  const double step = cam.span * 0.8 * dt;
  using K = ge::platform::Key;
  if (keys) {
    if (in.down(K::left) || in.down(K::a)) cam.cx -= step;
    if (in.down(K::right) || in.down(K::d)) cam.cx += step;
    if (in.down(K::up) || in.down(K::w)) cam.cy += step;
    if (in.down(K::down) || in.down(K::s)) cam.cy -= step;
    if (in.held[kHidMinus]) cam.span *= 1 + 1.2 * dt;
    if (in.held[kHidEquals]) cam.span /= 1 + 1.2 * dt;
    cam.span = std::clamp(cam.span, 256.0, 16384.0);
  }
  // A drag with the left button pans the map under the pointer.
  const float x = in.pointer_x * px_per_unit, y = in.pointer_y * px_per_unit;
  if (in.pointer_buttons & 1u) {
    if (cam.drag_x >= 0) {
      const double units_per_px = cam.span / std::max(1, h);
      cam.cx -= (x - cam.drag_x) * units_per_px;
      cam.cy += (y - cam.drag_y) * units_per_px;
    }
    cam.drag_x = x;
    cam.drag_y = y;
  } else {
    cam.drag_x = cam.drag_y = -1;
  }
}

ge::render::FrameView bot_cam_view(BotCam& cam, const sn::BotTeam& t, int w, int h, double t_s) {
  ge::render::FrameView v;
  if (t.nav && !cam.fitted) fit(cam, t);
  float top = 512;
  if (t.nav) top = t.nav->nav.graph.hi.z;
  // The map's part of the picture is left of the panel: its middle shifted so the map sits there.
  const double shift = static_cast<double>(kPanel) * 0.5 / std::max(1, h) * cam.span;
  const double height = cam.span * 0.5 / std::tan(kFov * 0.5);
  const ge::camera::Vec3 eye =
      engine_point({static_cast<float>(cam.cx + shift), static_cast<float>(cam.cy),
                    static_cast<float>(top + height)});
  v.eye[0] = eye.x;
  v.eye[1] = eye.y;
  v.eye[2] = eye.z;
  v.heading = engine_heading(90);  // the map's +y up the picture
  v.pitch = engine_pitch(89.9f);   // straight down
  v.fov_y = kFov;
  v.time_s = t_s;
  (void)w;
  return v;
}

void draw_bot_view(ge::ui::DrawList& o, const ge::render::FrameView& view, int w, int h,
                   const sn::BotTeam& t, const sn::TeamAreas& areas) {
  const ge::ui::Font& font = ge::ui::ui_font();
  const double now = static_cast<double>(t.now_us) / 1e6;
  Projector pr;
  pr.m = ge::render::camera_matrices(view, static_cast<double>(w) / std::max(1, h));
  pr.eye[0] = view.eye[0];
  pr.eye[1] = view.eye[1];
  pr.eye[2] = view.eye[2];
  pr.w = w;
  pr.h = h;
  const sn::BotSeat* first = nullptr;
  for (const auto& s : t.seats)
    if (s->set_up && s->fighter && s->mirror) {
      first = s.get();
      break;
    }

  if (t.nav && areas.node_area.size() == t.nav->nav.graph.nodes.size()) {
    const sh::NavGraph& g = t.nav->nav.graph;
    // The floor in its areas' colours, the lower floors first.
    std::vector<int32_t> order(g.nodes.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = static_cast<int32_t>(i);
    std::sort(order.begin(), order.end(), [&](int32_t a, int32_t b) {
      return g.nodes[static_cast<size_t>(a)].origin.z < g.nodes[static_cast<size_t>(b)].origin.z;
    });
    const float half = static_cast<float>(g.set.spacing) * 0.5f;
    for (const int32_t n : order) {
      const sh::Vec3f c = g.nodes[static_cast<size_t>(n)].origin;
      const sh::Vec3f floor{c.x, c.y, c.z - 24};
      Vec2 a, b;
      if (!pr.at({floor.x - half, floor.y + half, floor.z}, a) ||
          !pr.at({floor.x + half, floor.y - half, floor.z}, b))
        continue;
      const int8_t liquid = areas.node_liquid[static_cast<size_t>(n)];
      const Color col = liquid == sh::kContentsWater   ? kWater
                        : liquid == sh::kContentsSlime ? kSlime
                        : liquid != 0                  ? kLava
                                                       : area_colour(sn::team_area_state(
                                            areas, areas.node_area[static_cast<size_t>(n)], now));
      o.rect({a.x, a.y, b.x - a.x, b.y - a.y}, col);
    }
    // The areas' names, in their state's colour, where their first point stands.
    for (size_t i = 0; i < areas.points.size(); i++) {
      Vec2 p;
      if (!pr.at(areas.points[i].at, p)) continue;
      const sn::AreaState st = sn::team_area_state(areas, static_cast<int32_t>(i), now);
      if (st == sn::AreaState::unknown) continue;
      Color c = area_colour(st);
      c.a = 1;
      o.text(font, 12, p.x, p.y, areas.points[i].name, c, ge::ui::Align::center);
    }
    // The plan's base, forward area and rushes, ringed.
    if (t.nav->plan_made) {
      const bt::MapPlan& plan = t.nav->plan;
      const auto ring = [&](int32_t a, const char* what, Color c) {
        if (a < 0) return;
        Vec2 p;
        if (!pr.at(plan.areas[static_cast<size_t>(a)].at, p)) return;
        o.arc(p, 26, 3, 0, 6.3f, ge::ui::solid(c));
        o.text(font, 13, p.x, p.y - 32, what, c, ge::ui::Align::center);
      };
      ring(plan.base, "BASE", {0.2f, 1, 0.4f, 1});
      ring(plan.forward, "FORWARD", {0.6f, 1, 0.6f, 1});
      for (const int32_t r : plan.rushes) ring(r, "RUSH", {0.75f, 0.45f, 1, 1});
    }
    // The key items as the team believes them: filled while some bot believes one there, the
    // soonest return believed while taken.
    if (first) {
      const bt::Fighter& f0 = *first->fighter;
      for (size_t i = 0; i < f0.items.size(); i++) {
        const bt::ItemSpot& it = f0.items[i];
        if (!bt::key_item(it) && !(it.kind == bt::ItemKind::weapon && it.weapon >= 16)) continue;
        bool there = false;
        double back = 1e9;
        for (const auto& s : t.seats) {
          if (!s->set_up || !s->fighter || i >= s->fighter->items.size()) continue;
          const bt::ItemSpot& b = s->fighter->items[i];
          if (b.present)
            there = true;
          else
            back = std::min(back, b.back_at - s->mirror->s.sv.time);
        }
        Vec2 p;
        if (!pr.at(it.at, p)) continue;
        const Color c{1, 0.9f, 0.3f, 1};
        if (there)
          o.rect({p.x - 6, p.y - 6, 12, 12}, c, 2);
        else
          o.arc(p, 7, 2, 0, 6.3f, ge::ui::solid(c));
        std::string label = bt::item_call(it);
        if (!there && back < 1e8)
          label += " " + std::to_string(static_cast<int>(std::max(0.0, back)));
        o.text(font, 12, p.x + 10, p.y + 4, label, c);
      }
    }
    // Each bot: its believed enemies, its route, its goal, itself.
    int k = 0;
    for (const auto& sp : t.seats) {
      const Color col = kBots[k++ % 8];
      if (!sp->set_up || !sp->fighter || !sp->mirror) continue;
      const sn::BotSeat& s = *sp;
      const bt::Fighter& f = *s.fighter;
      const double tn = s.mirror->s.sv.time;
      const sh::Edict& me = s.mirror->s.sv.edicts[static_cast<size_t>(s.c.playernum) + 1];
      Vec2 m;
      if (!pr.at(me.origin, m)) continue;
      for (const sh::Contact& c : f.bot.senses.contacts) {
        if (c.ent < 0) continue;
        const double age = tn - c.seen.at;
        if (age < 6) {
          Vec2 p;
          if (!pr.at({static_cast<float>(c.seen.x), static_cast<float>(c.seen.y),
                      static_cast<float>(c.seen.z)},
                     p))
            continue;
          Color e = kEnemy;
          e.a = static_cast<float>(1 - age / 6);
          o.arc(p, c.visible ? 9 : 7, c.visible ? 9 : 3, 0, 6.3f, ge::ui::solid(e));
          o.line(m, p, 1, {col.r, col.g, col.b, 0.35f});
        }
      }
      // Its path: the line it means to walk, its lane off the route in (the console's `paths`).
      if (t.cfg.paths) {
        std::vector<uint8_t> breaks;
        const std::vector<sh::Vec3f> path = sh::pilot_path(f.bot.pilot.pilot, me.origin, &breaks);
        std::vector<Vec2> run;  // drawn a stretch at a time, broken at a teleporter
        for (size_t i = 0; i <= path.size(); i++) {
          Vec2 p;
          const bool end = i == path.size() || breaks[i] || !pr.at(path[i], p);
          if (end) {
            if (run.size() > 1) o.polyline(run, 2, {col.r, col.g, col.b, 0.85f});
            run.clear();
            if (i < path.size() && pr.at(path[i], p)) run.push_back(p);
            continue;
          }
          run.push_back(p);
        }
      }
      if (f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size()) {
        Vec2 p;
        if (pr.at(f.items[static_cast<size_t>(f.goal_item)].at, p))
          o.arc(p, 13, 2, 0, 6.3f, ge::ui::solid(col));
      }
      const double yaw = me.v_angle.y * 3.14159265358979 / 180.0;
      o.line(m,
             {m.x + 16 * static_cast<float>(std::cos(yaw)),
              m.y - 16 * static_cast<float>(std::sin(yaw))},
             3, col);
      o.arc(m, 6, 12, 0, 6.3f, ge::ui::solid(me.health > 0 ? col : kDim));
      o.text(font, 13, m.x, m.y - 14, s.name, col, ge::ui::Align::center);
    }
    // The enemies the team's clients were sent this tick, together (no single bot knows this).
    for (int slot = 0; slot < 32; slot++) {
      for (const auto& s : t.seats) {
        if (!s->set_up || !s->mirror || !s->fighter) continue;
        const sh::Edict& e = s->mirror->s.sv.edicts[static_cast<size_t>(slot) + 1];
        if (e.free || slot == s->c.playernum || (s->fighter->bot.senses.friends >> slot & 1u) ||
            e.health <= 0)
          continue;
        Vec2 p;
        if (!pr.at(e.origin, p)) break;
        o.line({p.x - 7, p.y - 7}, {p.x + 7, p.y + 7}, 3, {1, 0.6f, 0.1f, 1});
        o.line({p.x - 7, p.y + 7}, {p.x + 7, p.y - 7}, 3, {1, 0.6f, 0.1f, 1});
        const std::string& info = s->c.userinfos[slot];
        o.text(font, 11, p.x, p.y + 18, sn::info_get(info, "name"), {1, 0.6f, 0.1f, 1},
               ge::ui::Align::center);
        break;
      }
    }
  }

  // The panel.
  const float x0 = static_cast<float>(w - kPanel) + 14;
  o.rect({static_cast<float>(w - kPanel), 0, static_cast<float>(kPanel), static_cast<float>(h)},
         {0.05f, 0.055f, 0.07f, 0.9f});
  float y = 26;
  int ours = 0, theirs = 0;
  if (!t.seats.empty()) {
    const sn::ClassicClientEnd& c = t.seats[0]->c;
    for (int i = 0; i < 32; i++) {
      if (c.userinfos[i].empty() || !sn::info_get(c.userinfos[i], "*spectator").empty()) continue;
      (sn::info_get(c.userinfos[i], "team") == c.cfg.team ? ours : theirs) += c.frags[i];
    }
  }
  o.text(font, 20, x0, y, "TEAM " + t.cfg.team + "   " + (t.map.empty() ? "-" : t.map), kInk);
  y += 24;
  o.text(font, 18, x0, y,
         clock_text(now) + "    frags " + std::to_string(ours) + " : " + std::to_string(theirs),
         kInk);
  y += 26;
  int k = 0;
  for (const auto& sp : t.seats) {
    const Color col = kBots[k++ % 8];
    const sn::BotSeat& s = *sp;
    o.rect({x0, y - 10, 9, 9}, col, 2);
    std::string head = s.name + "  " + sh::aim_skill_name(s.skill);
    if (!s.set_up || !s.fighter) {
      o.text(font, 13, x0 + 14, y, head + "  " + sn::client_stage_name(s.c.stage), kDim);
      y += 34;
      continue;
    }
    const bt::Fighter& f = *s.fighter;
    const sn::ClassicClientEnd& c = s.c;
    const sh::Vec3f at = s.mirror->s.sv.edicts[static_cast<size_t>(c.playernum) + 1].origin;
    head += "   h" + std::to_string(c.stats[0]) + " a" + std::to_string(c.stats[4]) + " " +
            weapon_name(c.stats[10]) + "   frags " + std::to_string(c.frags[c.playernum]);
    o.text(font, 13, x0 + 14, y, head, kInk);
    std::string goal = "-";
    if (f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size())
      goal = bt::item_call(f.items[static_cast<size_t>(f.goal_item)]);
    o.text(font, 12, x0 + 14, y + 14,
           std::string(bt::fight_mode_name(f.mode)) + " / " + bt::role_name(f.role) + "   goal " +
               goal + "   at " + (f.locs ? sh::loc_name_at(*f.locs, at) : std::string("?")),
           kDim);
    o.text(font, 12, x0 + 14, y + 28,
           std::string("next ") + sh::pilot_mode_name(f.bot.pilot.pilot.mode) + ", " +
               std::to_string(f.bot.pilot.pilot.route.size() > f.bot.pilot.pilot.k
                                  ? f.bot.pilot.pilot.route.size() - f.bot.pilot.pilot.k
                                  : 0) +
               " nodes to go",
           kDim);
    y += 46;
  }
  // The plan.
  if (t.nav && t.nav->plan_made) {
    const bt::MapPlan& plan = t.nav->plan;
    const auto name = [&](int32_t a) {
      return a < 0 ? std::string("-") : plan.areas[static_cast<size_t>(a)].name;
    };
    std::string rush;
    for (const int32_t r : plan.rushes) rush += (rush.empty() ? "" : ", ") + name(r);
    o.text(font, 13, x0, y,
           "PLAN  base " + name(plan.base) + "   forward " + name(plan.forward) + "   rush " +
               (rush.empty() ? "-" : rush),
           {0.6f, 1, 0.6f, 1});
    y += 20;
  }
  // The legend.
  const std::pair<Color, const char*> legend[] = {{kSafe, "safe"},
                                                  {kLost, "lost"},
                                                  {kContested, "contested"},
                                                  {kUnknown, "unknown"},
                                                  {kWater, "water"}};
  float lx = x0;
  for (const auto& [c, n] : legend) {
    Color s = c;
    s.a = 1;
    o.rect({lx, y - 10, 10, 10}, s, 2);
    lx += 14 + o.text(font, 12, lx + 14, y, n, kDim) + 10;
  }
  y += 22;
  // The team's lines, newest at the bottom.
  o.text(font, 14, x0, y, "TEAM SAYS", kInk);
  y += 18;
  const size_t room = static_cast<size_t>(std::max(0.f, (static_cast<float>(h) - y - 8) / 15));
  const size_t from = areas.feed.size() > room ? areas.feed.size() - room : 0;
  for (size_t i = from; i < areas.feed.size(); i++) {
    const sn::TeamLineSeen& l = areas.feed[i];
    o.text(font, 12, x0, y, clock_text(l.t) + "  " + l.who + ": " + l.text, kDim);
    y += 15;
  }
}

}  // namespace shooters_edge::client
