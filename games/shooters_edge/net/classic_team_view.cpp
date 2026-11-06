// The team's world, pictured (classic_team_view.hpp).
//
// ge:summary The team's world pictured: map, bots, routes, goals, believed enemies, items, panel
// ge:keywords team-view floor items contacts route panel video
// ge:skill bots net labs
#include "shooters_edge/net/classic_team_view.hpp"

#include <algorithm>
#include <cmath>

#include "core/num_text.hpp"
#include "shooter/locs.hpp"
#include "shooter/pilot.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace {

namespace gl = ge::lab;
using Kind = bt::ItemKind;

constexpr int kPanel = 400;  // the panel's width, px
const gl::Rgba kBack{24, 26, 30, 255};
const gl::Rgba kInk{230, 230, 225, 255};
const gl::Rgba kDim{140, 140, 140, 255};
const gl::Rgba kEnemy{235, 60, 50, 255};
const gl::Rgba kSent{255, 150, 30, 255};
const gl::Rgba kBots[8] = {{80, 200, 255, 255},  {120, 255, 120, 255}, {255, 220, 80, 255},
                           {230, 120, 255, 255}, {255, 255, 255, 255}, {80, 255, 220, 255},
                           {255, 160, 160, 255}, {180, 180, 255, 255}};

gl::Rgba item_colour(Kind k) {
  switch (k) {
    case Kind::armor1:
      return {60, 200, 60, 255};
    case Kind::armor2:
      return {240, 210, 40, 255};
    case Kind::armor3:
      return {240, 60, 40, 255};
    case Kind::mega:
      return {60, 120, 255, 255};
    case Kind::quad:
      return {150, 80, 255, 255};
    case Kind::pent:
      return {255, 60, 160, 255};
    case Kind::weapon:
      return {200, 200, 200, 255};
    default:
      return {120, 120, 120, 255};
  }
}

bool key(const bt::ItemSpot& it) {
  return it.kind == Kind::armor1 || it.kind == Kind::armor2 || it.kind == Kind::armor3 ||
         it.kind == Kind::mega || it.kind == Kind::quad || it.kind == Kind::pent ||
         (it.kind == Kind::weapon && it.weapon >= 32);
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

std::string clock_text(double s) {
  const int t = static_cast<int>(s);
  char b[16];
  std::snprintf(b, sizeof b, "%d:%02d", t / 60, t % 60);
  return b;
}

void floor_for(TeamView& v, const BotTeam& t, int w, int h) {
  const sh::NavGraph& g = t.nav->nav.graph;
  v.map = t.map;
  v.lo = g.lo;
  v.hi = g.hi;
  v.map_w = w - kPanel;
  v.map_h = h;
  const double sx = (v.map_w - 40) / std::max(1.0, static_cast<double>(v.hi.x - v.lo.x));
  const double sy = (v.map_h - 40) / std::max(1.0, static_cast<double>(v.hi.y - v.lo.y));
  v.scale = std::min(sx, sy);
  v.floor = gl::create_picture(v.map_w, v.map_h, kBack);
  float zlo = 1e9f, zhi = -1e9f;
  for (const sh::NavNode& n : g.nodes) {
    zlo = std::min(zlo, n.origin.z);
    zhi = std::max(zhi, n.origin.z);
  }
  const double cell = std::max(2.0, 32 * v.scale);
  for (const sh::NavNode& n : g.nodes) {
    const double f = zhi > zlo ? (n.origin.z - zlo) / (zhi - zlo) : 0.5;
    const auto c = static_cast<uint8_t>(50 + 90 * f);
    const double x = 20 + (n.origin.x - v.lo.x) * v.scale, y = 20 + (v.hi.y - n.origin.y) * v.scale;
    gl::fill_rect(v.floor, x - cell / 2, y - cell / 2, cell, cell,
                  {c, c, static_cast<uint8_t>(c + 8), 255});
  }
}

}  // namespace

gl::Picture team_view(TeamView& v, const BotTeam& t, int w, int h) {
  gl::Picture p = gl::create_picture(w, h, kBack);
  const double now = static_cast<double>(t.now_us) / 1e6;
  if (t.nav && t.loaded) {
    if (v.map != t.map || v.floor.width != w - kPanel || v.floor.height != h) floor_for(v, t, w, h);
    gl::blit(p, v.floor, 0, 0);
  }
  const auto px = [&](double x) { return 20 + (x - v.lo.x) * v.scale; };
  const auto py = [&](double y) { return 20 + (v.hi.y - y) * v.scale; };
  const BotSeat* first = nullptr;
  for (const auto& s : t.seats)
    if (s->set_up && s->fighter && s->mirror) {
      first = s.get();
      break;
    }

  if (first && t.nav) {
    const sh::NavGraph& g = t.nav->nav.graph;
    // The items as the team believes them.
    const bt::Fighter& f0 = *first->fighter;
    for (size_t i = 0; i < f0.items.size(); i++) {
      const bt::ItemSpot& it = f0.items[i];
      if (!key(it)) continue;
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
      const gl::Rgba c = item_colour(it.kind);
      const double x = px(it.at.x), y = py(it.at.y);
      if (there) {
        gl::fill_rect(p, x - 6, y - 6, 12, 12, c);
      } else {
        gl::draw_ring(p, x, y, 7, c);
        if (back < 1e8)
          gl::draw_text(p, x + 9, y + 4, std::to_string(static_cast<int>(std::max(0.0, back))), c);
      }
      gl::draw_text(p, x + 9, y - 6, bt::item_call(it), c);
    }
    // The enemies the team's clients were sent this tick, together.
    for (int slot = 0; slot < 32; slot++) {
      bool enemy_sent = false;
      sh::Vec3f at;
      for (const auto& s : t.seats) {
        if (!s->set_up || !s->mirror) continue;
        const sh::Edict& e = s->mirror->s.sv.edicts[static_cast<size_t>(slot) + 1];
        if (e.free || slot == s->c.playernum || (s->fighter->bot.senses.friends >> slot & 1u))
          continue;
        if (e.health <= 0) continue;
        enemy_sent = true;
        at = e.origin;
      }
      if (!enemy_sent) continue;
      const double x = px(at.x), y = py(at.y);
      gl::draw_line(p, x - 6, y - 6, x + 6, y + 6, kSent, 2);
      gl::draw_line(p, x - 6, y + 6, x + 6, y - 6, kSent, 2);
    }
    // Each bot: its believed enemies, its route, its goal, itself.
    int k = 0;
    for (const auto& sp : t.seats) {
      const gl::Rgba col = kBots[k++ % 8];
      if (!sp->set_up || !sp->fighter || !sp->mirror) continue;
      const BotSeat& s = *sp;
      const bt::Fighter& f = *s.fighter;
      const double tn = s.mirror->s.sv.time;
      const sh::Edict& me = s.mirror->s.sv.edicts[static_cast<size_t>(s.c.playernum) + 1];
      const double mx = px(me.origin.x), my = py(me.origin.y);
      for (const sh::Contact& c : f.bot.senses.contacts) {
        if (c.ent < 0) continue;
        const double seen_age = tn - c.seen.at, heard_age = tn - c.heard.at;
        if (seen_age < 6) {
          const auto a = static_cast<uint8_t>(255 * (1 - seen_age / 6));
          const double x = px(c.seen.x), y = py(c.seen.y);
          gl::fill_disc(p, x, y, c.visible ? 6 : 4, {kEnemy.r, kEnemy.g, kEnemy.b, a});
          gl::draw_line(p, mx, my, x, y, {col.r, col.g, col.b, 90}, 1);
        } else if (heard_age < 6) {
          gl::draw_ring(p, px(c.heard.x), py(c.heard.y), 6, kEnemy);
        }
      }
      const sh::Pilot& pl = f.bot.pilot.pilot;
      double lx = mx, ly = my;
      for (size_t i = pl.k + 1; i < pl.route.size(); i++) {
        const sh::Vec3f n = g.nodes[static_cast<size_t>(pl.route[i])].origin;
        gl::draw_line(p, lx, ly, px(n.x), py(n.y), col, 2);
        lx = px(n.x);
        ly = py(n.y);
      }
      if (f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size()) {
        const bt::ItemSpot& it = f.items[static_cast<size_t>(f.goal_item)];
        gl::draw_ring(p, px(it.at.x), py(it.at.y), 11, col);
      }
      const double yaw = me.v_angle.y * 3.14159265358979 / 180.0;
      gl::draw_line(p, mx, my, mx + 14 * std::cos(yaw), my - 14 * std::sin(yaw), col, 2);
      gl::fill_disc(p, mx, my, 7, me.health > 0 ? col : kDim);
      gl::draw_text(p, mx - 2, my - 3, std::to_string(s.index + 1), kBack);
    }
  }

  // The panel.
  const int x0 = w - kPanel + 12;
  gl::fill_rect(p, w - kPanel, 0, kPanel, h, {16, 17, 20, 255});
  int y = 10;
  int ours = 0, theirs = 0;
  if (!t.seats.empty()) {
    const ClassicClientEnd& c = t.seats[0]->c;
    for (int i = 0; i < 32; i++) {
      if (c.userinfos[i].empty() || !info_get(c.userinfos[i], "*spectator").empty()) continue;
      (info_get(c.userinfos[i], "team") == c.cfg.team ? ours : theirs) += c.frags[i];
    }
  }
  gl::draw_text(p, x0, y, "TEAM " + t.cfg.team + "  " + (t.map.empty() ? "-" : t.map), kInk, 2);
  y += 20;
  gl::draw_text(
      p, x0, y,
      clock_text(now) + "   FRAGS " + std::to_string(ours) + " : " + std::to_string(theirs), kInk,
      2);
  y += 26;
  int k = 0;
  for (const auto& sp : t.seats) {
    const gl::Rgba col = kBots[k++ % 8];
    const BotSeat& s = *sp;
    gl::fill_rect(p, x0, y + 1, 8, 8, col);
    std::string head = s.name + " " + sh::aim_skill_name(s.skill);
    if (!s.set_up || !s.fighter) {
      gl::draw_text(p, x0 + 12, y, head + "  " + client_stage_name(s.c.stage), kDim);
      y += 30;
      continue;
    }
    const bt::Fighter& f = *s.fighter;
    const ClassicClientEnd& c = s.c;
    const sh::Vec3f at = s.mirror->s.sv.edicts[static_cast<size_t>(c.playernum) + 1].origin;
    head += "  H" + std::to_string(c.stats[0]) + " A" + std::to_string(c.stats[4]) + " " +
            weapon_name(c.stats[10]) + "  F" + std::to_string(c.frags[c.playernum]);
    gl::draw_text(p, x0 + 12, y, head, kInk);
    std::string goal = "-";
    if (f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size())
      goal = bt::item_call(f.items[static_cast<size_t>(f.goal_item)]);
    gl::draw_text(p, x0 + 12, y + 10,
                  std::string(bt::fight_mode_name(f.mode)) + "/" + bt::role_name(f.role) +
                      " GOAL " + goal + " AT " +
                      (f.locs ? sh::loc_name_at(*f.locs, at) : std::string("?")),
                  kDim);
    int seen = 0;
    for (const sh::Contact& ct : f.bot.senses.contacts) seen += ct.ent >= 0 && ct.visible ? 1 : 0;
    gl::draw_text(p, x0 + 12, y + 20,
                  std::string("NEXT ") + sh::pilot_mode_name(f.bot.pilot.pilot.mode) + ", " +
                      std::to_string(f.bot.pilot.pilot.route.size() > f.bot.pilot.pilot.k
                                         ? f.bot.pilot.pilot.route.size() - f.bot.pilot.pilot.k
                                         : 0) +
                      " NODES, SEES " + std::to_string(seen),
                  kDim);
    y += 34;
  }
  y += 6;
  gl::draw_text(p, x0, y, "TEAM SAYS", kInk);
  y += 12;
  const std::vector<bt::TeamLine>& log = t.team.log;
  const size_t from = log.size() > 14 ? log.size() - 14 : 0;
  for (size_t i = from; i < log.size() && y < h - 10; i++) {
    std::string who = "?";
    for (const auto& s : t.seats)
      if (s->c.playernum == log[i].slot) who = s->name;
    gl::draw_text(p, x0, y, clock_text(log[i].t) + " " + who + ": " + log[i].text, kDim);
    y += 10;
  }
  return p;
}

bool team_view_video_open(TeamViewVideo& v, const std::string& file, int w, int h, double fps,
                          std::string& error) {
  ge::capture::AvExportOptions o;
  o.file = file;
  o.width = w;
  o.height = h;
  o.fps = fps;
  o.codec = "auto";
  if (!v.out.open(o, error)) return false;
  v.open = true;
  return true;
}

bool team_view_video_frame(TeamViewVideo& v, const gl::Picture& p, std::string& error) {
  if (!v.open) return true;
  v.frames++;
  return v.out.frame(p, error);
}

bool team_view_video_close(TeamViewVideo& v, std::string& error) {
  if (!v.open) return true;
  v.open = false;
  return v.out.close(error);
}

}  // namespace shooters_edge::net
