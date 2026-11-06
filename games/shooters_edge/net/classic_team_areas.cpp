// The map's areas as a bot team believes them (classic_team_areas.hpp).
//
// ge:summary The map's areas as a bot team believes them: safe, lost, contested, unknown; the feed
// ge:keywords team-areas cut-areas node-area liquids news feed
// ge:skill bots net
#include "shooters_edge/net/classic_team_areas.hpp"

#include <algorithm>
#include <cmath>

#include "shooter/hull.hpp"
#include "shooter/world.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/talk.hpp"
#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace {

constexpr double kSeen_s = 1;  // an enemy seen this lately is where it was seen

// The areas cut for the team's map: one a place name, each nav node in its nearest point's.
void cut(TeamAreas& a, const BotTeam& t) {
  a = TeamAreas{};
  a.map = t.map;
  const sh::NavGraph& g = t.nav->nav.graph;
  const sh::Locs& locs = t.nav->locs;
  std::vector<int32_t> point_area(locs.points.size(), -1);
  for (size_t i = 0; i < locs.points.size(); i++) {
    const int32_t k = team_area_named(a, locs.points[i].name);
    if (k >= 0) {
      point_area[i] = k;
      continue;
    }
    point_area[i] = static_cast<int32_t>(a.points.size());
    a.points.push_back(locs.points[i]);
  }
  a.node_area.assign(g.nodes.size(), -1);
  a.node_liquid.assign(g.nodes.size(), 0);
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sh::Vec3f o = g.nodes[n].origin;
    const sh::Loc* l = sh::nearest_loc(locs, o);
    if (l) a.node_area[n] = point_area[static_cast<size_t>(l - locs.points.data())];
    const int c = sh::water_contents(t.nav->nav.world, {o.x, o.y, o.z - 23});
    if (c <= sh::kContentsWater) a.node_liquid[n] = static_cast<int8_t>(c);
  }
  a.safe_t.assign(a.points.size(), -1e9);
  a.lost_t.assign(a.points.size(), -1e9);
  a.fight_t.assign(a.points.size(), -1e9);
}

int32_t area_at(const TeamAreas& a, const BotTeam& t, sh::Vec3f p) {
  const int32_t n = sh::nav_nearest(t.nav->nav.graph, p);
  return n >= 0 && static_cast<size_t>(n) < a.node_area.size() ? a.node_area[static_cast<size_t>(n)]
                                                                 : -1;
}

void mark(std::vector<double>& v, int32_t i, double t) {
  if (i >= 0 && static_cast<size_t>(i) < v.size()) v[static_cast<size_t>(i)] = std::max(v[static_cast<size_t>(i)], t);
}

}  // namespace

const char* area_state_name(AreaState s) {
  switch (s) {
    case AreaState::safe: return "safe";
    case AreaState::lost: return "lost";
    case AreaState::contested: return "contested";
    default: return "unknown";
  }
}

int32_t team_area_named(const TeamAreas& a, const std::string& name) {
  for (size_t i = 0; i < a.points.size(); i++)
    if (a.points[i].name == name) return static_cast<int32_t>(i);
  return -1;
}

void team_areas_update(TeamAreas& a, const BotTeam& t) {
  if (!t.nav || t.map.empty()) return;
  if (a.map != t.map || a.node_area.size() != t.nav->nav.graph.nodes.size()) cut(a, t);
  const double now = static_cast<double>(t.now_us) / 1e6;
  // The team's bots: where they stand, what they fight, where they fell.
  for (const auto& sp : t.seats) {
    const BotSeat& s = *sp;
    if (!s.set_up || !s.fighter || !s.mirror) continue;
    const bt::Fighter& f = *s.fighter;
    const sh::Edict& me = s.mirror->s.sv.edicts[static_cast<size_t>(s.c.playernum) + 1];
    const int32_t here = area_at(a, t, me.origin);
    bool sees = false;
    for (const sh::Contact& c : f.bot.senses.contacts) {
      if (c.ent < 0 || !ge::ai::known(c.seen) || now - c.seen.at > kSeen_s) continue;
      sees = sees || c.visible;
      const sh::Vec3f at{static_cast<float>(c.seen.x), static_cast<float>(c.seen.y),
                         static_cast<float>(c.seen.z)};
      const int32_t there = area_at(a, t, at);
      if (f.mode == bt::FightMode::fight && c.visible) {
        mark(a.fight_t, there, now);
        mark(a.fight_t, here, now);
      } else {
        mark(a.lost_t, there, now);
      }
    }
    if (me.health > 0 && !sees) mark(a.safe_t, here, now);
    for (const auto& [where, when] : f.danger) {
      const int32_t k = area_at(a, t, where);
      mark(a.lost_t, k, when);
      mark(a.fight_t, k, when);
    }
  }
  // The team's lines.
  if (!t.seats.empty()) {
    const ClassicClientEnd& c = t.seats[0]->c;
    if (a.spawncount != c.spawncount) {
      a.spawncount = c.spawncount;
      a.prints_read = std::min(a.prints_read, c.prints.size());
    }
    for (; a.prints_read < c.prints.size(); a.prints_read++) {
      std::string who, words;
      if (!bt::split_team_line(c.prints[a.prints_read], who, words)) continue;
      while (!words.empty() && (words.back() == '\n' || words.back() == ' ')) words.pop_back();
      a.feed.push_back({now, who, words});
      if (a.feed.size() > 200) a.feed.erase(a.feed.begin());
      const bt::Said said = bt::parse_say(words);
      const int32_t k = said.loc.empty() ? -1 : team_area_named(a, said.loc);
      switch (said.kind) {
        case bt::SayKind::safe: mark(a.safe_t, k, now); break;
        case bt::SayKind::lost:
        case bt::SayKind::enemy:
        case bt::SayKind::enemy_power: mark(a.lost_t, k, now); break;
        case bt::SayKind::help: mark(a.fight_t, k, now); break;
        default: break;
      }
    }
  }
}

AreaState team_area_state(const TeamAreas& a, int32_t i, double now) {
  if (i < 0 || static_cast<size_t>(i) >= a.points.size()) return AreaState::unknown;
  const size_t k = static_cast<size_t>(i);
  if (now - a.fight_t[k] < kContested_s) return AreaState::contested;
  const double news = std::max(a.safe_t[k], a.lost_t[k]);
  if (now - news > kNews_s) return AreaState::unknown;
  return a.lost_t[k] > a.safe_t[k] ? AreaState::lost : AreaState::safe;
}

}  // namespace shooters_edge::net
