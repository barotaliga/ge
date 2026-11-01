// A fighter's own mind in a team game (mind.hpp): team lines read by skill into its beliefs, and
// its role and its calls planned off them alone.
//
// ge:summary A fighter's own mind: lines read by skill, beliefs of mates, its role and its calls
// ge:keywords mind_hear mind_plan mind_say gives_up mate_going place_of status_of read-chance
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/mind.hpp"

#include <cmath>

#include "shooter/senses.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/team.hpp"

namespace shooters_edge::bots {

namespace {

constexpr int32_t kItQuad = 4194304;  // the quad's IT_ bit (games/shooters_edge/qc)
constexpr int32_t kWRockets = 32, kWArc = 64;
constexpr double kHelp_s = 6;         // a call for help holds this long
constexpr double kHoldsFresh_s = 40;  // a mate's `safe` this old still says he holds his area
constexpr double kGoingFresh_s = 12;  // a mate's `coming X` this old still says he goes for X
constexpr double kToldFresh_s = 10;   // a point this old is still followed
constexpr float kMateSight = 1500;    // a teammate is made out within this, units
constexpr float kGiveNear = 600;      // an item is left to a teammate in sight this near it
constexpr float kQuadNear = 800;      // a teammate this near the quad has it covered
constexpr float kSafeNear = 250;      // a holder this near his item calls it safe

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

double draw(Fighter& f) {
  f.rng = f.rng * 1664525u + 1013904223u;
  return static_cast<double>(f.rng >> 8) / 16777216.0;
}

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

// Where a place name is: the middle of the points of that name, else the item called by it
// nearest `near` (the speaker's own place when it has one).
bool place_pos(const Fighter& f, const std::string& name, sh::Vec3f& out) {
  if (name.empty()) return false;
  if (f.locs) {
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

bool alive_slot(const server::QcServer& s, int slot) {
  return slot >= 0 && s.sv.clients[static_cast<size_t>(slot)].active &&
         s.sv.edicts[static_cast<size_t>(slot) + 1].health > 0;
}

// A teammate's slot in sight from `me`'s eye within `kMateSight`.
bool sees_mate(const Fighter& f, const server::QcServer& s, int me, int slot) {
  if (!alive_slot(s, slot)) return false;
  const sh::Edict& self = s.sv.edicts[static_cast<size_t>(me)];
  sh::SenseSettings look = f.bot.senses.set;
  look.range = kMateSight;
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + look.view_height};
  return sh::sees_player(s.sv, look, me, eye, {f.bot.pitch, f.bot.yaw, 0},
                         s.sv.edicts[static_cast<size_t>(slot) + 1].origin);
}

// Whether a teammate is believed fresh within `r` of `p` (in sight now or said lately).
bool mate_near(const Fighter& f, sh::Vec3f p, float r, double t) {
  for (const Mate& m : f.mind.mates)
    if (t - m.at_t < kMateFresh_s && dist(m.at, p) < r) return true;
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
    if (f.items[i].kind == ItemKind::weapon && f.items[i].weapon == kWRockets &&
        f.items[i].node >= 0)
      return static_cast<int32_t>(i);
  return -1;
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
  out.weapon = (st.items & kWRockets) ? "rl" : (st.items & kWArc) ? "lg" : "";
  if (out.weapon.empty()) out.weapon = (st.items & 16) ? "gl" : (st.items & 2) ? "ssg" : "sg";
  out.quad = (st.items & kItQuad) != 0;
  return out;
}

std::string place_of(const Fighter& f, sh::Vec3f p) {
  return f.locs ? sh::loc_name_at(*f.locs, p) : std::string();
}

bool mind_say(Fighter& f, double t, Said said, sh::Vec3f at) {
  if (said.loc.empty() && said.kind != SayKind::get && said.kind != SayKind::over &&
      said.kind != SayKind::timer)
    said.loc = place_of(f, at);
  return team_say(f, t, say_text(said));
}

bool mate_going(const Fighter& f, int32_t i, double t) {
  for (const Mate& m : f.mind.mates)
    if (m.going == i && t - m.going_t < kGoingFresh_s) return true;
  return false;
}

void mind_hear(Fighter& f, const std::string& from, const std::string& text, double t) {
  if (from == f.name) return;  // its own words, sent back to it
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
  if (f.sense == MapSense::average) {
    chance = count >= 2 ? 0.9 : fighting ? 0.25 : 0.5;
  } else if (f.sense == MapSense::novice) {
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
  const bool has_place = place_pos(f, s.loc, p);
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
      if (has_place) {
        m.at = p;
        m.at_t = t;
      }
      break;
    default:
      break;
  }
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
        it.timed = f.sense != MapSense::novice;
        double back = it.respawn_s * (it.timed ? 1.0 : kGuessLate);
        if (f.sense == MapSense::average && key_item(it)) back += (draw(f) - 0.5) * 10;
        it.back_at = t + back;
        it.known = it.known || f.sense != MapSense::novice;
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
      }
      break;
    case SayKind::help:
      if (has_place) {
        mn.help_at = p;
        mn.help_until = t + kHelp_s;
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
    case SayKind::none:
    case SayKind::report:
      break;
  }
}

bool gives_up(const Fighter& f, const server::QcServer& s, int me, int32_t i, const Stats& st,
              double t) {
  if (!f.team) return false;
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
    spare = st.rockets >= 50;
  }
  // The quad's carrier's due: the best armour, the launcher, the lightning gun.
  const bool for_quad =
      !mine_quad && mate_quad(f, t) &&
      (it.kind == ItemKind::armor3 ||
       (it.kind == ItemKind::weapon && (it.weapon == kWRockets || it.weapon == kWArc)));
  if (!spare && !for_quad) return false;
  // Left only to a teammate in sight near it (the carrier anywhere believed near).
  for (const Fighter* m : f.team->members) {
    if (m->slot < 0 || m->slot == me - 1) continue;
    if (!sees_mate(f, s, me, m->slot)) continue;
    if (dist(s.sv.edicts[static_cast<size_t>(m->slot) + 1].origin, it.at) < kGiveNear) return true;
  }
  // Or one it believes near it (a report's place, a sighting a moment ago).
  return mate_near(f, it.at, for_quad ? 1200 : kGiveNear, t);
}

void mind_plan(Fighter& f, const server::QcServer& s, int me, const Stats& st, double t) {
  Mind& mn = f.mind;
  if (!f.team || t < mn.next_plan) return;
  if (mn.next_plan == 0) {
    // Each player's plans fall at their own moment, as people's do (none in step with another).
    mn.next_plan = t + kPlan_s * draw(f);
    return;
  }
  mn.next_plan = t + kPlan_s;
  const sh::Edict& self = s.sv.edicts[static_cast<size_t>(me)];
  // Its first plan alive with its reach known: the match's start.
  const bool opening = !mn.opened && self.health > 0 && f.reach_at >= 0;
  if (opening) mn.opened = true;
  const sh::Vec3f at = self.origin;
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
    mn.claimed_said = false;
    return;
  }
  // Its mates in sight: where they are, now.
  for (const Fighter* m : f.team->members) {
    if (m->slot < 0 || m->slot == me - 1 || !sees_mate(f, s, me, m->slot)) continue;
    Mate& b = mate_named(f, m->name);
    b.slot = m->slot;
    b.at = s.sv.edicts[static_cast<size_t>(m->slot) + 1].origin;
    b.at_t = t;
  }

  const bool quad_mine = (st.items & kItQuad) != 0;
  const float strong = fighter_strength(st);
  const int32_t q = first_of(f, ItemKind::quad);
  // The areas worth holding: the best armour's, the launcher's and the quad's; the mega health's
  // in place of the armour's on a map whose armour no way leads to.
  int32_t ra = best_armour(f);
  if (!reachable(f, ra)) ra = first_of(f, ItemKind::mega);
  const int32_t rl = rocket_launcher(f);

  // The claim: at the start the nearest of the launcher, the best armour and the quad that no
  // mate said he is coming for; later the best armour's area when no mate is believed to hold it.
  if (mn.claim >= 0 && mn.claim == q && !f.items[static_cast<size_t>(q)].present && !quad_mine)
    mn.claim = -1;  // the quad gone: the claim is free again
  if (mn.claim < 0 && f.sense != MapSense::novice && !quad_mine && f.reach_at >= 0) {
    const auto taken = [&](int32_t i) {
      for (const Mate& m : mn.mates)
        if ((m.holds == i && t - m.holds_t < kHoldsFresh_s) ||
            (m.going == i && t - m.going_t < kGoingFresh_s))
          return true;
      return false;
    };
    int32_t pick = -1;
    if (opening || f.sense == MapSense::expert) {
      float best = 0;
      for (int32_t i : {rl, ra, q}) {
        if (!reachable(f, i) || taken(i)) continue;
        if (!opening && i != ra) continue;  // later only the armour's area is taken on
        const float d = dist(f.items[static_cast<size_t>(i)].at, at);
        if (pick < 0 || d < best) {
          pick = i;
          best = d;
        }
      }
    } else if (reachable(f, ra) && !taken(ra) && strong >= kHoldStrong) {
      pick = ra;
    }
    if (pick >= 0 && (opening || strong >= kHoldStrong)) {
      mn.claim = pick;
      mn.claimed_said = false;
    }
  }
  if (mn.claim >= 0 && !mn.claimed_said) {
    Said c;
    c.kind = SayKind::coming;
    c.item = item_call(f.items[static_cast<size_t>(mn.claim)]);
    if (mind_say(f, t, c, at)) mn.claimed_said = true;
  }

  // The role.
  Role r = Role::free;
  if (quad_mine) {
    r = Role::carry;
  } else if (q >= 0 && reachable(f, q) && due_of(f, q, t) - t <= kRushAhead_s) {
    // Among the two of its team it believes nearest the quad (those going for it counted).
    const float mine = dist(at, f.items[static_cast<size_t>(q)].at);
    int ahead = 0;
    for (const Mate& m : mn.mates)
      if ((t - m.at_t < kMateFresh_s && dist(m.at, f.items[static_cast<size_t>(q)].at) < mine) ||
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
  if (r == Role::free && mn.claim >= 0) r = Role::hold;
  if (r == Role::rush && was != Role::rush) f.team->stats.rushes++;
  if (r == Role::carry && was != Role::carry) {
    f.team->stats.quads++;
    Said h;
    h.kind = SayKind::have;
    h.item = "quad";
    mind_say(f, t, h, at);
  }
  f.role = r;

  // The calls. Its status now and then, as players press their report key.
  if (t - mn.reported_at >= kReport_s[static_cast<int>(f.sense)]) {
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
  if (f.sense == MapSense::expert && t - mn.timer_call_at >= 5) {
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
    if (i < 0 || f.sense == MapSense::novice) return;
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
