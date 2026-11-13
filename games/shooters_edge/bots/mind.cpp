// A fighter's own mind in a team game (mind.hpp): team lines read by skill into its beliefs, and
// its role and its calls planned off them alone.
//
// ge:summary A fighter's own mind: lines read by skill, beliefs of mates, its role and its calls
// ge:keywords mind_hear mind_plan mind_say gives_up mate_going place_of status_of read-chance
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/mind.hpp"

#include <algorithm>
#include <cmath>

#include "core/prng.hpp"
#include "shooter/senses.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_plan.hpp"
#include "shooters_edge/bots/team.hpp"

namespace shooters_edge::bots {

namespace {

constexpr size_t kDangerKept = 12;  // the danger spots kept (mind_danger)

constexpr int32_t kItQuad = 4194304;  // the quad's IT_ bit (games/shooters_edge/qc)
constexpr double kHelp_s = 6;         // a call for help holds this long
constexpr double kHoldsFresh_s = 40;  // a mate's `safe` this old still says he holds his area
constexpr double kPackClaim_s = 6;    // a bare `coming` this soon after a pack's call claims it
constexpr double kGoingFresh_s = 12;  // a mate's `coming X` this old still says he goes for X
constexpr float kMateSight = 1500;    // a teammate is made out within this, units
constexpr float kGiveNear = 600;      // an item is left to a teammate in sight this near it
constexpr float kQuadNear = 800;      // a teammate this near the quad has it covered
constexpr float kSafeNear = 250;      // a holder this near his item calls it safe
constexpr float kHelpNear = 700;      // help is called by a key item this near, enemies as near
constexpr double kHelpSeen_s = 2;     // an enemy seen this lately is about
constexpr float kCovered = 700;       // a key area a teammate is this near is his, not the enemy's
constexpr double kLostFresh_s = 8;    // an enemy in the base this lately makes it the enemy's
constexpr float kBaseReach = 600;     // an enemy this near the base's item is in the base
constexpr float kPushStrong = 130;    // strength to call a push; to join one, 0.8 of it
constexpr float kStageOut = 400;      // a gather spot at least this far from the base's item
constexpr float kGatherNear = 350;    // a mate this near the gather spot has gathered
constexpr double kGather_s = 8;       // the gather waits this long at most
constexpr double kPush_s = 30;        // a push lasts this long at most
constexpr float kPlaceLink = 400;     // `placeheard`: points of one name this near are one place
constexpr double kPushGap_s = 25;     // one player calls a push no oftener than this
constexpr double kPushClear_s = 4;    // on the base with no enemy seen this long: the push is over
constexpr double kFetchAhead_s = 5;   // the taker goes for an armour due this soon
constexpr double kFetch_s = 30;       // and holds the trip this long at most

// The protection an armour item gives (its kind's share of a blow times its value); 0 for others.
float armour_gives(const ItemSpot& it) {
  return it.kind == ItemKind::armor3   ? 0.8f * 200
         : it.kind == ItemKind::armor2 ? 0.6f * 150
         : it.kind == ItemKind::armor1 ? 0.3f * 100
                                       : 0.f;
}

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

// How long a teammate's place stays believed: an expert keeps his team's map fresh and drops a
// place he has not had news of for `kExpertMateFresh_s`.
double mate_fresh_s(const Fighter& f) {
  return f.comms == MapSense::expert ? kExpertMateFresh_s : kMateFresh_s;
}

double draw(Fighter& f) { return ge::core::next(f.rng); }

Mate& mate_named(Fighter& f, const std::string& name) {
  for (Mate& m : f.mind.mates)
    if (m.name == name) return m;
  f.mind.mates.push_back({});
  f.mind.mates.back().name = name;
  return f.mind.mates.back();
}

// The protection a status's armour gives (its kind's share of the damage times its value).
float protection_of(const SayStatus& st) {
  const float share = st.armor_kind == "ra" ? 0.8f : st.armor_kind == "ya" ? 0.6f : 0.3f;
  return st.armor < 0 ? 0.f : share * static_cast<float>(st.armor);
}

// A place heard by its name as a person hears it (`placeheard`): a loc file names a room by many
// points and the same name may stand for rooms apart, so the name is read as the points of that
// name linked to the one nearest `hint` (where the speaker was last known; none: the first), each
// within `kPlaceLink` of the next; and where an item stands among them (a key item, a weapon or
// the mega whose nearest point is one of them), as that item's spot, the one nearest the place's
// middle (what a map's items name: the place a player calls by it); else the place's point nearest
// its middle. False when no point has the name.
bool place_heard(const Fighter& f, const std::string& name, const sh::Vec3f* hint, sh::Vec3f& out) {
  const std::vector<sh::Loc>& pts = f.locs->points;
  std::vector<uint8_t> in(pts.size(), 0);
  size_t seed = pts.size();
  float seed_d = 0;
  for (size_t i = 0; i < pts.size(); i++) {
    if (pts[i].name != name) continue;
    const float d = hint ? dist(pts[i].at, *hint) : 0;
    if (seed == pts.size() || d < seed_d) {
      seed = i;
      seed_d = d;
    }
  }
  if (seed == pts.size()) return false;
  std::vector<size_t> todo{seed};
  in[seed] = 1;
  sh::Vec3f sum{0, 0, 0};
  int n = 0;
  while (!todo.empty()) {
    const size_t a = todo.back();
    todo.pop_back();
    sum = sum + pts[a].at;
    n++;
    for (size_t b = 0; b < pts.size(); b++)
      if (!in[b] && pts[b].name == name && dist(pts[a].at, pts[b].at) <= kPlaceLink) {
        in[b] = 1;
        todo.push_back(b);
      }
  }
  const sh::Vec3f mid = sum * (1.f / static_cast<float>(n));
  int32_t item = -1;
  float item_d = 0;
  for (size_t i = 0; i < f.items.size(); i++) {
    const ItemSpot& it = f.items[i];
    if (!key_item(it) && it.kind != ItemKind::weapon && it.kind != ItemKind::mega) continue;
    const sh::Loc* l = sh::nearest_loc(*f.locs, it.at);
    if (l == nullptr || !in[static_cast<size_t>(l - pts.data())]) continue;
    const float d = dist(it.at, mid);
    if (item < 0 || d < item_d) {
      item = static_cast<int32_t>(i);
      item_d = d;
    }
  }
  if (item >= 0) {
    out = f.items[static_cast<size_t>(item)].at;
    return true;
  }
  bool first = true;
  float best = 0;
  for (size_t i = 0; i < pts.size(); i++) {
    if (!in[i]) continue;
    const float d = dist(pts[i].at, mid);
    if (first || d < best) {
      first = false;
      best = d;
      out = pts[i].at;
    }
  }
  return true;
}

// Where a place name is: the middle of the points of that name (`placeheard`: the place round
// `hint`, place_heard), else the item called by it nearest `near` (the speaker's own place when
// it has one).
bool place_pos(const Fighter& f, const std::string& name, sh::Vec3f& out,
               const sh::Vec3f* hint = nullptr) {
  if (name.empty()) return false;
  if (f.locs && f.features[sh::Feature::placeheard]) {
    if (place_heard(f, name, hint, out)) return true;
  } else if (f.locs) {
    sh::Vec3f sum{0, 0, 0};
    int n = 0;
    for (const sh::Loc& l : f.locs->points)
      if (l.name == name) {
        sum = sum + l.at;
        n++;
      }
    if (n > 0) {
      out = sum * (1.f / static_cast<float>(n));
      return true;
    }
  }
  for (const ItemSpot& it : f.items)
    if (name == item_call(it)) {
      out = it.at;
      return true;
    }
  return false;
}

// The item a message names by its word, nearest `p` when the place is known.
int32_t item_named(const Fighter& f, const std::string& word, const sh::Vec3f* p) {
  int32_t best = -1;
  float best_d = 0;
  for (size_t i = 0; i < f.items.size(); i++) {
    if (word != item_call(f.items[i])) continue;
    const float d = p ? dist(f.items[i].at, *p) : 0;
    if (best < 0 || d < best_d) {
      best = static_cast<int32_t>(i);
      best_d = d;
    }
  }
  return best;
}

// The key item nearest `p` within `reach`, or -1.
int32_t key_near(const Fighter& f, sh::Vec3f p, float reach) {
  int32_t best = -1;
  float best_d = reach;
  for (size_t i = 0; i < f.items.size(); i++) {
    if (!key_item(f.items[i])) continue;
    const float d = dist(f.items[i].at, p);
    if (d < best_d) {
      best = static_cast<int32_t>(i);
      best_d = d;
    }
  }
  return best;
}

bool alive_slot(const server::RulesCommon& s, int slot) {
  return slot >= 0 && s.sv.clients[static_cast<size_t>(slot)].active &&
         s.world.entities[static_cast<size_t>(slot) + 1].health > 0;
}

// A teammate's slot in sight from `me`'s eye within `kMateSight`.
bool sees_mate(const Fighter& f, const server::RulesCommon& s, int me, int slot) {
  if (!alive_slot(s, slot)) return false;
  const server::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  sh::SenseSettings look = f.bot.senses.set;
  look.range = kMateSight;
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + look.view_height};
  return sh::sees_player(s.sv, look, me, eye, {f.bot.pitch, f.bot.yaw, 0},
                         s.world.entities[static_cast<size_t>(slot) + 1].origin);
}

// Whether a teammate is believed fresh within `r` of `p` (in sight now or said lately).
bool mate_near(const Fighter& f, sh::Vec3f p, float r, double t) {
  for (const Mate& m : f.mind.mates)
    if (t - m.at_t < mate_fresh_s(f) && dist(m.at, p) < r) return true;
  return false;
}

bool mate_quad(const Fighter& f, double t) {
  for (const Mate& m : f.mind.mates)
    if (m.quad && t - m.quad_t < 30) return true;
  return false;
}

int32_t first_of(const Fighter& f, ItemKind k) {
  for (size_t i = 0; i < f.items.size(); i++)
    if (f.items[i].kind == k && f.items[i].node >= 0) return static_cast<int32_t>(i);
  return -1;
}

// The best armour on the map (the 200, else the 150), -1 none.
int32_t best_armour(const Fighter& f) {
  const int32_t ra = first_of(f, ItemKind::armor3);
  return ra >= 0 ? ra : first_of(f, ItemKind::armor2);
}

int32_t rocket_launcher(const Fighter& f) {
  for (size_t i = 0; i < f.items.size(); i++)
    if (f.items[i].kind == ItemKind::weapon && f.items[i].role == WeaponRole::launcher &&
        f.items[i].node >= 0)
      return static_cast<int32_t>(i);
  return -1;
}

// How long before the quad is due its rushers go: the map plan's run from the base to the quad's
// area and its lead, else kRushAhead_s; never under kRushAhead_s / 2 nor over twice it.
double rush_ahead_s(const Fighter& f, int32_t q) {
  if (!f.plan) return kRushAhead_s;
  for (size_t r = 0; r < f.plan->rushes.size(); r++)
    for (const int32_t i : f.plan->areas[static_cast<size_t>(f.plan->rushes[r])].items)
      if (i == q) return std::clamp(f.plan->rush_leave_s[r], kRushAhead_s / 2, kRushAhead_s * 2);
  return kRushAhead_s;
}

// A way there by its links from where it stands (not known until its reach is worked out).
bool reachable(const Fighter& f, int32_t i) {
  return i >= 0 && f.items[static_cast<size_t>(i)].node >= 0 && f.reach_at >= 0 &&
         f.reach.reached(f.items[static_cast<size_t>(i)].node);
}

// When item `i` is believed due: now when believed there, its time back when believed gone and
// timed (or guessed), never when nothing is known.
double due_of(const Fighter& f, int32_t i, double t) {
  const ItemSpot& it = f.items[static_cast<size_t>(i)];
  if (!it.known && !it.in_sight) return 1e30;
  return it.present ? t : it.back_at;
}

// The run from `p` to node `to` over the nav's links, s (`ai::kUnreached` where none leads).
double run_s(const Fighter& f, sh::Vec3f p, int32_t to) {
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  ge::ai::Search search;
  sh::nav_reach(g, sh::nav_nearest(g, p), search);
  return search.cost(to);
}

// Whether the base round item `ra` is believed the enemy's: an enemy in it seen or called out
// within `kLostFresh_s` and no mate believed there, or worked out lost (`retake`).
bool base_lost(const Fighter& f, int32_t ra, sh::Vec3f at, double t) {
  const Mind& mn = f.mind;
  const sh::Vec3f there = f.items[static_cast<size_t>(ra)].at;
  if (dist(at, there) < kSafeNear || mate_near(f, there, kSafeNear * 2, t)) return false;
  if (mn.retake == ra && t - mn.retake_t < kLostFresh_s) return true;
  const auto in = [&](sh::Vec3f p, double when) {
    return t - when < kLostFresh_s && dist(p, there) < kBaseReach;
  };
  return in(mn.enemy_at, mn.enemy_t) || in(mn.sighted_at, mn.sighted_t);
}

// The place to gather at before a push on the base round item `ra`: of the places outside the
// map plan's ways into it (named, at least `kStageOut` from the item, a way leading there), the one
// nearest `at`. False with no plan or no such place.
bool gather_spot(const Fighter& f, int32_t ra, sh::Vec3f at, std::string& name, sh::Vec3f& pos) {
  if (!f.plan || f.plan->base < 0 || !f.bot.pilot.pilot.g) return false;
  const PlanArea& base = f.plan->areas[static_cast<size_t>(f.plan->base)];
  if (std::find(base.items.begin(), base.items.end(), ra) == base.items.end()) return false;
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  const sh::Vec3f there = f.items[static_cast<size_t>(ra)].at;
  const std::string inside = place_of(f, there);
  float best = 0;
  bool found = false;
  for (const int32_t e : base.entries) {
    const std::string n = place_of(f, g.nodes[static_cast<size_t>(e)].origin);
    sh::Vec3f p;
    if (n.empty() || n == inside || !place_pos(f, n, p, &g.nodes[static_cast<size_t>(e)].origin) ||
        dist(p, there) < kStageOut)
      continue;
    const int32_t node = sh::nav_nearest(g, p);
    if (node < 0 || (f.reach_at >= 0 && !f.reach.reached(node))) continue;
    const float d = dist(p, at);
    if (!found || d < best) {
      found = true;
      best = d;
      name = n;
      pos = p;
    }
  }
  return found;
}

void say_status(Fighter& f, double t, SayKind kind, const Stats& st, sh::Vec3f at) {
  Said s;
  s.kind = kind;
  s.status = status_of(st);
  mind_say(f, t, s, at);
}

}  // namespace

SayStatus status_of(const Stats& st) {
  SayStatus out;
  out.armor = static_cast<int>(st.armor);
  out.health = static_cast<int>(st.health);
  out.armor_kind = st.armortype >= 0.75f   ? "ra"
                   : st.armortype >= 0.55f ? "ya"
                   : st.armortype > 0      ? "ga"
                                           : "";
  const BotArsenal& a = *st.arsenal;
  bool dbl = false;
  for (const BotWeapon& w : a.weapons) dbl |= w.curve == WeaponCurve::double_ && (st.items & w.bit);
  out.weapon = (st.items & a.launcher) ? "rl" : (st.items & a.beam) ? "lg" : "";
  if (out.weapon.empty()) out.weapon = (st.items & a.lob) ? "gl" : dbl ? "ssg" : "sg";
  out.quad = (st.items & kItQuad) != 0;
  return out;
}

std::string place_of(const Fighter& f, sh::Vec3f p) {
  return f.locs ? sh::loc_name_at(*f.locs, p) : std::string();
}

bool mind_say(Fighter& f, double t, Said said, sh::Vec3f at) {
  // A novice just plays: now and then his status, as a key pressed, and nothing else.
  if (f.comms == MapSense::novice && said.kind != SayKind::report) return false;
  if (said.loc.empty() && said.kind != SayKind::get && said.kind != SayKind::over &&
      said.kind != SayKind::timer)
    said.loc = place_of(f, at);
  return team_say(f, t, say_text(said));
}

bool wants_help(const Fighter& f, sh::Vec3f at, bool weak, double t) {
  if (f.teamwork == MapSense::novice) return false;
  // The enemies about: seen in the last moments within reach of a fight.
  int near = 0;
  for (const sh::Contact& c : f.bot.senses.contacts)
    if (c.ent >= 0 && ge::ai::known(c.seen) && ge::ai::age(c.seen, t) < kHelpSeen_s &&
        dist({static_cast<float>(c.seen.x), static_cast<float>(c.seen.y),
              static_cast<float>(c.seen.z)},
             at) < kHelpNear)
      near++;
  if (near == 0) return false;
  if (f.teamwork == MapSense::expert) {
    // His own area being lost: two on him, or one while he is low.
    const int32_t area = f.mind.claim;
    if (f.role != Role::hold || area < 0 ||
        dist(at, f.items[static_cast<size_t>(area)].at) > kHelpNear)
      return false;
    return near >= 2 || weak;
  }
  // An average player calls whenever he is threatened by a key item.
  for (const ItemSpot& it : f.items)
    if (key_item(it) && dist(at, it.at) < kHelpNear) return true;
  return false;
}

bool mate_going(const Fighter& f, int32_t i, double t) {
  for (const Mate& m : f.mind.mates)
    if (m.going == i && t - m.going_t < kGoingFresh_s) return true;
  return false;
}

void mind_danger(Fighter& f, sh::Vec3f at, double t) {
  if (!f.features[sh::Feature::danger]) return;
  if (f.danger.size() >= kDangerKept) f.danger.erase(f.danger.begin());
  f.danger.emplace_back(at, t);
}

bool death_words(std::string_view rest) {
  // The words a death's line goes on with after the victim's name ("was nailed by", "ate 2 loads
  // of", "sleeps with the fishes", "becomes bored with life", ...).
  static constexpr const char* kDeathWords[] = {
      "was ",       "ate ",    "rides ", "accepts ", "chewed ", "eats ",    "gets ", "sleeps ",
      "sucks ",     "gulped ", "burst ", "visits ",  "turned ", "cratered", "fell ", "becomes ",
      "discovers ", "tries ",  "can't ", "squished", "loses ",  "checks ",  "died",  "got "};
  for (const char* w : kDeathWords)
    if (rest.rfind(w, 0) == 0) return true;
  return false;
}

bool mind_obituary(Fighter& f, const std::string& line, double t) {
  for (const Mate& m : f.mind.mates) {
    if (m.name.empty() || line.size() <= m.name.size() + 1 ||
        line.compare(0, m.name.size(), m.name) != 0 || line[m.name.size()] != ' ')
      continue;
    if (!death_words(std::string_view(line).substr(m.name.size() + 1))) return false;
    if (t - m.at_t < mate_fresh_s(f)) mind_danger(f, m.at, t);
    return true;
  }
  return false;
}

// The opening's item for this player (map_plan.hpp `opening`): down the plan's order, each item
// still there takes the players (this one, and each mate whose spawn it read) who reach it within
// its late run, the nearest first, up to its cap; -1 when none falls to this player.
int32_t opening_pick(const Fighter& f, double t) {
  if (!f.plan || f.plan->opening.empty() || f.reach_at < 0) return -1;
  struct Who {
    std::string name;
    sh::Vec3f at;
    bool me = false;
    bool taken = false;
  };
  std::vector<Who> team{{f.name, {}, true, false}};
  for (const Mate& m : f.mind.mates)
    if (t - m.at_t < mate_fresh_s(f)) team.push_back({m.name, m.at, false, false});
  for (const OpenItem& o : f.plan->opening) {
    const ItemSpot& it = f.items[static_cast<size_t>(o.item)];
    if (!it.present || !reachable(f, o.item)) continue;
    std::vector<std::pair<double, size_t>> near;
    for (size_t k = 0; k < team.size(); k++) {
      if (team[k].taken) continue;
      const double run = team[k].me ? f.reach.cost(it.node) : run_s(f, team[k].at, it.node);
      if (run <= o.late_s) near.push_back({run, k});
    }
    std::sort(near.begin(), near.end(), [&](const auto& a, const auto& b) {
      return a.first != b.first ? a.first < b.first : team[a.second].name < team[b.second].name;
    });
    for (size_t n = 0; n < near.size() && static_cast<int>(n) < o.cap; n++) {
      team[near[n].second].taken = true;
      if (team[near[n].second].me) return o.item;
    }
  }
  return -1;
}

bool mind_take_plan(Fighter& f, const Said& said, double t) {
  if (said.kind != SayKind::plan) return false;
  Mind& mn = f.mind;
  int32_t items[2] = {-1, -1};
  if (said.strategy != Strategy::free) {
    items[0] = item_named(f, said.item, nullptr);
    items[1] = said.item2.empty() ? -1 : item_named(f, said.item2, nullptr);
    if (items[0] < 0) return false;
  }
  mn.strategy = said.strategy;
  mn.strategy_items[0] = items[0];
  mn.strategy_items[1] = items[1];
  mn.strategy_t = t;
  mn.claim = -1;  // claimed again off the plan at the next plan
  mn.claimed_said = false;
  return true;
}

void mind_hear(Fighter& f, const std::string& from, const std::string& text, double t) {
  if (from == f.name || !f.features[sh::Feature::talk]) return;  // its own words, or deaf
  Mind& mn = f.mind;
  // The repeat count: the same words within the window.
  int count = 1;
  bool found = false;
  for (HeardLine& h : mn.heard)
    if (h.text == text && t - h.t < kRepeatWindow_s) {
      h.count++;
      h.t = t;
      count = h.count;
      found = true;
    }
  if (!found) mn.heard.push_back({text, t, 1});
  std::erase_if(mn.heard, [&](const HeardLine& h) { return t - h.t >= kRepeatWindow_s; });

  // Read, or missed, by how well it plays and whether it is fighting.
  const bool fighting = f.mode == FightMode::fight;
  double chance = 1;
  if (f.comms == MapSense::average) {
    chance = count >= 2 ? 0.9 : fighting ? 0.25 : 0.5;
  } else if (f.comms == MapSense::novice) {
    chance = count >= 3 ? (fighting ? 0.3 : 0.6) : fighting ? 0 : 0.15;
  }
  if (chance < 1 && draw(f) >= chance) {
    mn.missed++;
    return;
  }
  mn.read++;

  const Said s = parse_say(text);
  Mate& m = mate_named(f, from);
  sh::Vec3f p;
  // (`placeheard`) The place read round where the speaker was last known.
  const bool known = t - m.at_t < mate_fresh_s(f);
  const bool has_place = place_pos(f, s.loc, p, known ? &m.at : nullptr);
  const sh::Vec3f* pp = has_place ? &p : nullptr;
  // Where the speaker is: the place his own news carries.
  switch (s.kind) {
    case SayKind::report:
    case SayKind::took:
    case SayKind::safe:
    case SayKind::help:
    case SayKind::coming:
    case SayKind::have:
    case SayKind::lost:
    case SayKind::attack:
      if (has_place) {
        m.at = p;
        m.at_t = t;
      }
      break;
    default:
      break;
  }
  if (!s.status.weapon.empty()) {
    m.has_rl = s.status.weapon == "rl";
    m.weapon_t = t;
  }
  if (s.status.health >= 0) m.armour = protection_of(s.status);
  if (s.status.health >= 0) {
    m.strength = static_cast<float>(s.status.health) + protection_of(s.status) +
                 (s.status.weapon == "rl" ? 50.f : 0.f);
    m.strength_t = t;
  }
  if (s.status.quad) {
    m.quad = true;
    m.quad_t = t;
  }
  switch (s.kind) {
    case SayKind::took: {
      const int32_t i = item_named(f, s.item, pp);
      if (i >= 0) {
        ItemSpot& it = f.items[static_cast<size_t>(i)];
        it.present = it.confirmed = false;
        it.timed = f.timing != MapSense::novice;
        double back = it.respawn_s * (it.timed ? 1.0 : kGuessLate);
        if (f.timing == MapSense::average && key_item(it)) back += (draw(f) - 0.5) * 10;
        it.back_at = t + back;
        it.known = it.known || f.sense != MapSense::novice;
        mega_taken(f, i, t, false);
        if (it.kind == ItemKind::quad) {
          m.quad = true;
          m.quad_t = t;
        }
      }
      break;
    }
    case SayKind::timer: {
      const int32_t i = item_named(f, s.item, nullptr);
      if (i >= 0) {
        ItemSpot& it = f.items[static_cast<size_t>(i)];
        it.present = it.confirmed = false;
        it.timed = true;
        it.back_at = t + s.secs;
      }
      break;
    }
    case SayKind::coming: {
      // Right after a pack's call, a bare `coming [ga]` (or `coming rl`) answers it: that mate
      // takes the pack, the rest let it be (packs.hpp `packs_goal`).
      if (t - mn.pack_t < kPackClaim_s && (s.item.empty() || s.item == "rl" || s.item == "lg")) {
        mn.pack_claim_t = t;
        m.going = -1;
        m.going_t = t;
        break;
      }
      int32_t i = s.item.empty() ? -1 : item_named(f, s.item, nullptr);
      if (i < 0 && has_place) i = key_near(f, p, 600);
      m.going = i;
      m.going_t = t;
      break;
    }
    case SayKind::safe:
      if (has_place) {
        m.holds = key_near(f, p, 600);
        m.holds_t = t;
      }
      break;
    case SayKind::lost:
      m.holds = -1;
      if (has_place) {
        mn.enemy_at = p;
        mn.enemy_t = t;
        mind_danger(f, p, t);  // no healing there for a while
        // An area to take back (what an average player goes by: he keeps no map of his team).
        const int32_t area = key_near(f, p, 600);
        if (area >= 0 && f.teamwork != MapSense::novice) {
          mn.retake = area;
          mn.retake_t = t;
        }
      }
      break;
    case SayKind::help:
      if (has_place) {
        mn.help_at = p;
        mn.help_until = t + kHelp_s;
        mind_danger(f, p, t);  // a mate in trouble there: no healing there for a while
      }
      break;
    case SayKind::enemy:
    case SayKind::enemy_power:
      if (has_place) {
        mn.enemy_at = p;
        mn.enemy_t = t;
      }
      break;
    case SayKind::have:
      m.quad = true;
      m.quad_t = t;
      break;
    case SayKind::over:
      m.quad = false;
      break;
    case SayKind::point: {
      // "You take it": taken by the one who believes himself the nearest of his team to it.
      const int32_t i = item_named(f, s.item, pp);
      if (i < 0) break;
      mn.told = i;
      mn.told_t = t;
      break;
    }
    case SayKind::get: {
      const int32_t i = item_named(f, s.item, nullptr);
      if (i >= 0) {
        ItemSpot& it = f.items[static_cast<size_t>(i)];
        it.known = true;  // a novice told where the quad is learns it
        if (!it.present && it.back_at > t + 3) it.back_at = t + 3;
      }
      break;
    }
    case SayKind::attack: {
      // "We take it back together: gather on me": joined by a player strong enough to.
      const int32_t i = item_named(f, s.item, nullptr);
      if (i < 0 || !has_place || f.teamwork == MapSense::novice || f.role == Role::carry ||
          f.role == Role::rush || (mn.push_item >= 0 && t - mn.push_t < kPush_s))
        break;
      mn.push_item = i;
      mn.push_t = t;
      mn.push_at = p;
      mn.push_go_t = -1e9;
      if (f.team) f.team->stats.joins++;
      break;
    }
    case SayKind::plan:
      if (mind_take_plan(f, s, t) && draw(f) < kAckChance) mn.ack_due = true;
      break;
    case SayKind::pack:
      if (has_place) {
        mn.pack_at = p;
        mn.pack_t = t;
        mn.pack_rl = s.item == "rl";
      }
      break;
    case SayKind::none:
    case SayKind::report:
    case SayKind::ack:
      break;
  }
}

bool gives_up(const Fighter& f, const server::RulesCommon& s, int me, int32_t i, const Stats& st,
              double t) {
  if (!f.team || f.teamwork == MapSense::novice) return false;  // a novice takes what he finds
  if (ge::shooter::selfish(f.traits)) return false;             // he takes all he can carry
  const ItemSpot& it = f.items[static_cast<size_t>(i)];
  const bool mine_quad = (st.items & kItQuad) != 0;
  // No use to this player.
  bool spare = false;
  if (it.kind == ItemKind::armor1 || it.kind == ItemKind::armor2 || it.kind == ItemKind::armor3) {
    const float give = it.kind == ItemKind::armor3   ? 0.8f * 200
                       : it.kind == ItemKind::armor2 ? 0.6f * 150
                                                     : 0.3f * 100;
    spare = st.armortype * st.armor >= give * 0.9f;
  } else if (it.kind == ItemKind::weapon) {
    spare = (st.items & it.weapon) != 0;
  } else if (it.kind == ItemKind::rockets) {
    spare = ammo_of(st, st.arsenal->launcher_ammo) >= 50;
  }
  // The quad's carrier's due: the best armour, the launcher, the lightning gun.
  const bool for_quad = !mine_quad && mate_quad(f, t) &&
                        (it.kind == ItemKind::armor3 ||
                         (it.kind == ItemKind::weapon &&
                          (it.role == WeaponRole::launcher || it.role == WeaponRole::beam)));
  if (!spare && !for_quad) return false;
  // Left only to a teammate in sight near it (the carrier anywhere believed near).
  for (const Fighter* m : f.team->members) {
    if (m->slot < 0 || m->slot == me - 1) continue;
    if (!sees_mate(f, s, me, m->slot)) continue;
    if (dist(s.world.entities[static_cast<size_t>(m->slot) + 1].origin, it.at) < kGiveNear)
      return true;
  }
  // Or one it believes near it (a report's place, a sighting a moment ago).
  return mate_near(f, it.at, for_quad ? 1200 : kGiveNear, t);
}

void mind_plan(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t) {
  Mind& mn = f.mind;
  if (!f.team || t < mn.next_plan) return;
  if (mn.next_plan == 0) {
    // Each player's plans fall at their own moment, as people's do (none in step with another).
    mn.next_plan = t + kPlan_s * draw(f);
    return;
  }
  mn.next_plan = t + kPlan_s;
  const server::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  // Its first plan alive with its reach known: the match's start. Its spawn is said first, and
  // the opening's claims wait for the next plan, the team's spawns read.
  const sh::Vec3f at = self.origin;
  if (!mn.opened && self.health > 0 && f.reach_at >= 0) {
    mn.opened = mn.opening_due = true;
    say_status(f, t, SayKind::report, st, at);
    mn.reported_at = t;
    return;
  }
  bool opening = mn.opening_due && self.health > 0;
  mn.opening_due = false;
  const Role was = f.role;
  if (self.health <= 0) {
    if (was == Role::carry) {
      Said over;
      over.kind = SayKind::over;
      over.item = "quad";
      mind_say(f, t, over, at);
    }
    f.role = Role::free;
    mn.claim = -1;
    if (mn.fetch >= 0) f.team->stats.fetch_end[3]++;
    mn.fetch = -1;
    mn.claimed_said = false;
    return;
  }
  // Its mates in sight: where they are, now.
  for (const Fighter* m : f.team->members) {
    if (m->slot < 0 || m->slot == me - 1 || !sees_mate(f, s, me, m->slot)) continue;
    Mate& b = mate_named(f, m->name);
    b.slot = m->slot;
    b.at = s.world.entities[static_cast<size_t>(m->slot) + 1].origin;
    b.at_t = t;
  }

  const bool quad_mine = (st.items & kItQuad) != 0;
  const float strong = fighter_strength(st);
  const int32_t q = first_of(f, ItemKind::quad);
  // The areas worth holding: the best armour's, the launcher's and the quad's; on a map whose best
  // armour no way leads to, the best armour a way does lead to (a team holds an armour, not the
  // mega health: holding the mega draws both teams' hurt players into one room), the mega's only
  // when no armour is reachable.
  int32_t ra = best_armour(f);
  if (!reachable(f, ra) && f.plan && f.plan->base >= 0) {
    // The map's own plan (map_plan.hpp): its base's best item a way leads to.
    ra = -1;
    double worth = 0;
    for (const int32_t i : f.plan->areas[static_cast<size_t>(f.plan->base)].items)
      if (reachable(f, i) && plan_worth(f.items[static_cast<size_t>(i)]) > worth) {
        worth = plan_worth(f.items[static_cast<size_t>(i)]);
        ra = i;
      }
  }
  if (!reachable(f, ra)) {
    ra = -1;
    for (const ItemKind k : {ItemKind::armor3, ItemKind::armor2, ItemKind::armor1})
      for (size_t i = 0; i < f.items.size() && ra < 0; i++)
        if (f.items[i].kind == k && reachable(f, static_cast<int32_t>(i)))
          ra = static_cast<int32_t>(i);
    if (ra < 0) ra = first_of(f, ItemKind::mega);
  }
  const int32_t rl = rocket_launcher(f);

  // The claim: at the start the nearest of the launcher, the best armour and the quad that no
  // mate said he is coming for; later the best armour's area when no mate is believed to hold it.
  if (mn.claim >= 0 && mn.claim == q && !f.items[static_cast<size_t>(q)].present && !quad_mine) {
    // The quad gone: a rusher takes up the opening's claim at once (the nearest of the launcher
    // and the armour no mate holds), so the team still holds its key areas.
    mn.claim = -1;
    opening = true;
  }
  if (mn.claim < 0 && f.teamwork != MapSense::novice && !quad_mine && f.reach_at >= 0) {
    const auto taken = [&](int32_t i) {
      for (const Mate& m : mn.mates)
        if ((m.holds == i && t - m.holds_t < kHoldsFresh_s) ||
            (m.going == i && t - m.going_t < kGoingFresh_s))
          return true;
      return false;
    };
    int32_t pick = -1;
    // The opening's quad first: claimed by the two of the team who believe themselves nearest it
    // (a mate's spawn as he said it, or one who said he is coming for it, counted ahead).
    if (opening && f.plan && !f.plan->opening.empty()) {
      // The map's opening order (map_plan.hpp): the first item still to be had in time.
      pick = opening_pick(f, t);
    } else if (opening && q >= 0 && reachable(f, q) && f.items[static_cast<size_t>(q)].present) {
      // By the run over the map's links, not the line through its walls.
      const int32_t qn = f.items[static_cast<size_t>(q)].node;
      const double mine = f.reach.cost(qn);
      int ahead = 0;
      for (const Mate& m : mn.mates)
        if ((t - m.at_t < mate_fresh_s(f) && run_s(f, m.at, qn) < mine) ||
            (m.going == q && t - m.going_t < kGoingFresh_s))
          ahead++;
      if (ahead < 2) pick = q;
    }
    if (pick < 0 && (opening || f.teamwork == MapSense::expert)) {
      float best = 0;
      for (int32_t i : {rl, ra}) {
        if (!reachable(f, i) || taken(i)) continue;
        if (!opening && i != ra) continue;  // later only the armour's area is taken on
        // The nearest, as far as the map's rating of it goes (a lone 200 is worth a longer run).
        const float d =
            dist(f.items[static_cast<size_t>(i)].at, at) /
            (f.sense == MapSense::expert ? f.items[static_cast<size_t>(i)].weight : 1.f);
        if (pick < 0 || d < best) {
          pick = i;
          best = d;
        }
      }
    } else if (pick < 0 && reachable(f, ra) && !taken(ra) && strong >= kHoldStrong) {
      pick = ra;
    }
    if (pick >= 0 && (opening || strong >= kHoldStrong)) {
      mn.claim = pick;
      mn.claimed_said = false;
    }
  }
  if (mn.claim >= 0 && !mn.claimed_said) {
    if (opening) f.said_at = -1e9;  // the opening's claim follows its spawn at once
    Said c;
    c.kind = SayKind::coming;
    c.item = item_call(f.items[static_cast<size_t>(mn.claim)]);
    if (mind_say(f, t, c, at)) mn.claimed_said = true;
  }

  // The role.
  Role r = Role::free;
  if (quad_mine) {
    r = Role::carry;
  } else if (q >= 0 && f.teamwork != MapSense::novice && reachable(f, q) &&
             due_of(f, q, t) - t <= rush_ahead_s(f, q)) {
    // Among the two of its team it believes nearest the quad (those going for it counted).
    const float mine = dist(at, f.items[static_cast<size_t>(q)].at);
    int ahead = 0;
    for (const Mate& m : mn.mates)
      if ((t - m.at_t < mate_fresh_s(f) && dist(m.at, f.items[static_cast<size_t>(q)].at) < mine) ||
          (m.going == q && t - m.going_t < kGoingFresh_s))
        ahead++;
    if (ahead < 2) r = Role::rush;
  }
  // A rush begun is said, so the rest of the team leave the quad to it.
  if (r == Role::rush && was != Role::rush) {
    Said c;
    c.kind = SayKind::coming;
    c.item = "quad";
    mind_say(f, t, c, at);
  }
  // An aggressive player holds nothing: he goes where the frags are.
  if (r == Role::free && mn.claim >= 0 && !ge::shooter::selfish(f.traits)) r = Role::hold;
  // The team's strategy over its own plan (above): take an item together, or camp one or two.
  if (mn.strategy == Strategy::take && t - mn.strategy_t >= kTake_s) mn.strategy = Strategy::none;
  if (mn.strategy == Strategy::camp && t - mn.strategy_t >= kCamp_s) mn.strategy = Strategy::none;
  if (mn.strategy == Strategy::free) mn.strategy = Strategy::none;
  if (mn.strategy != Strategy::none && !quad_mine && r != Role::carry) {
    int32_t mine = mn.strategy_items[0];
    if (mn.strategy == Strategy::camp && mn.strategy_items[1] >= 0) {
      // Split by the roster's slot order: the first, third ... take the first item.
      int order = 0;
      for (const Fighter* m : f.team->members)
        if (m->slot >= 0 && m->slot < me - 1) order++;
      if (order % 2 == 1) mine = mn.strategy_items[1];
    }
    if (mine >= 0 && reachable(f, mine)) {
      if (mn.claim != mine) {
        mn.claim = mine;
        mn.claimed_said = false;
      }
      r = mine == q && due_of(f, q, t) - t <= rush_ahead_s(f, q) ? Role::rush : Role::hold;
    }
  }
  if (mn.ack_due && f.said_at < t - 1) {
    Said a;
    a.kind = SayKind::ack;
    a.variant = static_cast<int>(draw(f) * 4);
    mind_say(f, t, a, at);
    mn.ack_due = false;
  }
  // The push (above): called on the base believed the enemy's, joined off a mate's call; over
  // on the base with nobody seen a while, or once it has run its time.
  if (mn.push_item >= 0 && t - mn.push_t >= kPush_s) mn.push_item = -1;
  if (mn.push_item >= 0 && dist(at, f.items[static_cast<size_t>(mn.push_item)].at) < kSafeNear &&
      t - mn.sighted_t > kPushClear_s)
    mn.push_item = -1;
  if (r == Role::free || r == Role::hold) {
    std::string name;
    sh::Vec3f spot;
    if (mn.push_item < 0 && f.teamwork != MapSense::novice && strong >= kPushStrong && ra >= 0 &&
        reachable(f, ra) && t - mn.push_said_at >= kPushGap_s && base_lost(f, ra, at, t) &&
        gather_spot(f, ra, at, name, spot)) {
      Said a;
      a.kind = SayKind::attack;
      a.item = item_call(f.items[static_cast<size_t>(ra)]);
      a.loc = name;
      if (mind_say(f, t, a, at)) {
        mn.push_item = ra;
        mn.push_t = mn.push_said_at = t;
        mn.push_at = spot;
        mn.push_go_t = -1e9;
        f.team->stats.pushes++;
      }
    }
    if (mn.push_item >= 0 && strong >= kPushStrong * 0.8f) {
      r = Role::push;
      // In once a mate has gathered with it, or the gather has waited its time.
      if (mn.push_go_t < 0) {
        const bool here = dist(at, mn.push_at) < kGatherNear;
        bool mate = false;
        for (const Mate& m : mn.mates)
          mate = mate || (t - m.at_t < 2 && dist(m.at, mn.push_at) < kGatherNear);
        if ((here && (mate || t - mn.push_t >= kGather_s)) || t - mn.push_t >= 2 * kGather_s)
          mn.push_go_t = t;
      }
    }
  }
  if (r == Role::rush && was != Role::rush) f.team->stats.rushes++;
  if (r == Role::carry && was != Role::carry) {
    f.team->stats.quads++;
    Said h;
    h.kind = SayKind::have;
    h.item = "quad";
    mind_say(f, t, h, at);
  }
  f.role = r;

  // THE TAKER (above): the team's armour taken by the nearest who needs it, the trip held.
  if (f.features[sh::Feature::fetch]) {
    const float mine = st.armortype * st.armor;
    if (mn.fetch >= 0) {
      const float gives = armour_gives(f.items[static_cast<size_t>(mn.fetch)]);
      const bool worn = mine >= gives * 0.9f;
      const int end = worn                                         ? -1
                      : r == Role::carry || r == Role::rush        ? 0
                      : due_of(f, mn.fetch, t) - t > kFetchAhead_s ? 1
                      : t - mn.fetch_t > kFetch_s                  ? 2
                                                                   : 5;
      if (worn) f.team->stats.fetched++;
      if (end >= 0 && end < 5) f.team->stats.fetch_end[end]++;
      if (end < 5) mn.fetch = -1;
    }
    if (mn.fetch < 0 && ra >= 0 && f.teamwork != MapSense::novice && r != Role::carry &&
        r != Role::rush && reachable(f, ra) && !mate_going(f, ra, t)) {
      const ItemSpot& it = f.items[static_cast<size_t>(ra)];
      const float gives = armour_gives(it);
      if (gives > 0 && mine < gives * 0.9f && due_of(f, ra, t) - t <= kFetchAhead_s) {
        const double run = f.reach.cost(it.node);
        bool ahead = false;
        for (const Mate& m : mn.mates) {
          if (ahead || t - m.at_t >= mate_fresh_s(f)) continue;
          const bool needs =
              m.armour < 0 || t - m.strength_t >= kMateFresh_s || m.armour < gives * 0.9f;
          ahead = needs && run_s(f, m.at, it.node) < run;
        }
        if (!ahead) {
          mn.fetch = ra;
          mn.fetch_t = t;
          f.team->stats.fetches++;
          Said c;
          c.kind = SayKind::coming;
          c.item = item_call(it);
          mind_say(f, t, c, at);
        }
      }
    }
  }

  // An expert who knows where all his team is knows where it is not: a key area none of them is
  // in (nor he) is likely the enemy's, the most valuable of them the one to take back.
  if (f.comms == MapSense::expert) {
    int fresh = 0;
    for (const Mate& m : mn.mates) fresh += t - m.at_t < mate_fresh_s(f);
    const int others = static_cast<int>(f.team->members.size()) - 1;
    if (others > 0 && fresh >= others) {
      int32_t lost = -1;
      float worth = 0;
      for (int32_t i : {ra, rl, first_of(f, ItemKind::mega)}) {
        if (i < 0 || !reachable(f, i) || i == mn.claim) continue;
        const sh::Vec3f there = f.items[static_cast<size_t>(i)].at;
        if (mate_near(f, there, kCovered, t) || dist(at, there) < kCovered) continue;
        const float w = f.items[static_cast<size_t>(i)].weight * (i == ra ? 1.2f : 1.f);
        if (lost < 0 || w > worth) {
          lost = i;
          worth = w;
        }
      }
      if (lost >= 0 && lost != mn.retake) f.team->stats.lost_seen++;
      mn.retake = lost;
      mn.retake_t = lost >= 0 ? t : mn.retake_t;
    }
  }

  // The calls. Its status now and then, as players press their report key.
  if (t - mn.reported_at >= kReport_s[static_cast<int>(f.comms)]) {
    say_status(f, t, SayKind::report, st, at);
    mn.reported_at = t;
  }
  // Its area safe: on it, nobody in sight.
  if (r == Role::hold && mn.claim >= 0 &&
      dist(at, f.items[static_cast<size_t>(mn.claim)].at) < kSafeNear &&
      f.mode == FightMode::roam && t - mn.safe_at >= 20) {
    Said sf;
    sf.kind = SayKind::safe;
    sf.status = status_of(st);
    sf.loc = place_of(f, f.items[static_cast<size_t>(mn.claim)].at);
    if (mind_say(f, t, sf, at)) mn.safe_at = t;
  }
  // The timers it keeps, called as the item comes due (an expert's only: the rest guess).
  if (f.timing == MapSense::expert && t - mn.timer_call_at >= 5) {
    for (const ItemSpot& it : f.items) {
      if (!key_item(it) || it.present || !it.timed) continue;
      const double left = it.back_at - t;
      const bool power = it.kind == ItemKind::quad || it.kind == ItemKind::pent;
      if ((left > 9.5 && left <= 10.5) || (power && left > 19.5 && left <= 20.5)) {
        Said tm;
        tm.kind = SayKind::timer;
        tm.item = item_call(it);
        tm.secs = static_cast<int>(std::lround(left));
        if (mind_say(f, t, tm, at)) mn.timer_call_at = t;
        break;
      }
    }
  }
  // The powers: "get quad" when due, said again every few seconds while no mate is near it (the
  // more often the further away the team is), by whoever believes it due.
  const auto call_power = [&](int32_t i, double& last) {
    if (i < 0 || f.comms == MapSense::novice) return;
    const ItemSpot& it = f.items[static_cast<size_t>(i)];
    // Called by a player who timed it, as it comes due.
    if (it.present || !it.timed || it.back_at - t > 5) return;
    const sh::Vec3f there = f.items[static_cast<size_t>(i)].at;
    const bool covered = mate_near(f, there, kQuadNear, t) || dist(at, there) < kQuadNear;
    const double gap = covered ? 15 : mate_near(f, there, 2 * kQuadNear, t) ? 6 : 3;
    if (t - last < gap) return;
    Said g;
    g.kind = SayKind::get;
    g.item = item_call(f.items[static_cast<size_t>(i)]);
    if (mind_say(f, t, g, at)) last = t;
  };
  if (!quad_mine) call_power(q, mn.quad_call_at);
  call_power(first_of(f, ItemKind::pent), mn.pent_call_at);
  // An item here it leaves to a mate: pointed at, its status beside it.
  for (size_t i = 0; i < f.items.size(); i++) {
    const ItemSpot& it = f.items[i];
    if (!it.present || !it.in_sight || dist(it.at, at) > 400) continue;
    const auto ii = static_cast<int32_t>(i);
    if (!gives_up(f, s, me, ii, st, t) || (mn.gave == ii && t - mn.gave_at < 15)) continue;
    Said pt;
    pt.kind = SayKind::point;
    pt.item = item_call(it);
    pt.loc = place_of(f, it.at);
    if (mind_say(f, t, pt, at)) {
      mn.gave = ii;
      mn.gave_at = t;
      mn.reported_at = -1e9;  // the status follows it
    }
    break;
  }
}

}  // namespace shooters_edge::bots
