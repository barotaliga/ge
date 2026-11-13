// Backpacks (packs.hpp): every death's pack placed and guessed; the call, the fetch and the bore.
//
// ge:summary Backpacks: every death's pack placed and guessed; the call, the fetch and the bore
// ge:keywords packs_obituary packs_goal packs_bore packs_hear rl-pack pack_guess packs_truth
// ge:skill bots
// ge:test tests/shooters_edge_packs_test.cpp
#include "shooters_edge/bots/packs.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <string_view>
#include <vector>

#include "ai/memory.hpp"
#include "shooter/traits.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/bots/talk.hpp"
#include "shooters_edge/profiles/arsenal.hpp"

namespace shooters_edge::bots {

namespace {

namespace sh = ge::shooter;

constexpr double kPackSeen_s = 2;    // the victim's place known this lately: where the pack lies
constexpr float kPackReached = 48;   // a called pack this near is had (or gone)
constexpr double kBoreHold_s = 1.5;  // the bore's command held this long at most
constexpr double kPackCall_s = 3;    // a pack's call waits this long at most for its turn to speak

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

sh::Vec3f at_of(const ge::ai::Memory& m) {
  return {static_cast<float>(m.x), static_cast<float>(m.y), static_cast<float>(m.z)};
}

constexpr double kRlHeard_s = 4;     // without `packinfer`: a rocket heard this lately, a launcher
constexpr double kCryFresh_s = 1;    // a cry heard this lately places its player
constexpr double kShotLand_s = 3;    // its own shot's kill reckoned within this of the shot
constexpr double kBlastFresh_s = 1;  // and the blast heard within this of the kill
constexpr double kLanded_s = 1;      // a pack this old has come to rest (seen gone only after)
constexpr float kPackSame = 128;     // a pack seen this near a known one is that one
constexpr float kSeenGone = 600;     // a known pack's spot in sight this near, bare: gone
constexpr float kOnLine = 160;       // a blast this near its rocket's line was its rocket
constexpr float kOnArc = 300;        // and near a grenade's (it arcs and bounces)

bool is_backpack(const server::RulesCommon& s, const sh::ViewEntity& v) {
  return v.modelindex > 0 && static_cast<size_t>(v.modelindex) < s.models.size() &&
         s.models[static_cast<size_t>(v.modelindex)] == "progs/backpack.mdl";
}

// A spot on the floor in sight on its screen (no wall, door or water's surface between).
bool sees_spot(const Fighter& f, const server::RulesCommon& s, int me, sh::Vec3f eye,
               sh::Vec3f at) {
  const sh::SenseSettings& look = f.bot.senses.set;
  return sh::sees_player(s.sv, look, me, eye, {f.bot.pitch, f.bot.yaw, 0},
                         {at.x, at.y, at.z + 8 - look.view_height});
}

// A draw in [0, 1) off a pack's time and its victim: whether a player bothers with a lesser pack
// (its own stream of draws stays as it was).
double pack_draw(double t, int32_t victim, int slot) {
  uint64_t h = static_cast<uint64_t>(t * 1000.0) * 0x9E3779B97F4A7C15ull;
  h ^= static_cast<uint64_t>(victim + 64 * (slot + 1)) * 0xBF58476D1CE4E5B9ull;
  h ^= h >> 31;
  h *= 0x94D049BB133111EBull;
  h ^= h >> 29;
  return static_cast<double>(h >> 11) * 0x1.0p-53;
}

// The weapon number (1..8) of a W_ bit; 0 for none.
int32_t number_of_bit(int32_t bit) {
  for (int32_t n = 0; n < 8; n++)
    if (bit == (n == 0 ? 4096 : 1 << (n - 1))) return n + 1;
  return 0;
}

// How long after its last fire sound a weapon is surely still in hand: a weapon select waits until
// the refire is over (porting/shooters-edge/weapon-switch.md). A single shot's refire; a stream
// (the nailguns, the arc) ends 0.2 s after its last shot, and the arc sounds at most every 0.6 s
// while it fires, so its last bolt may come that much after the last sound heard.
double switch_window(int32_t n) {
  const shooters_edge::profiles::WeaponRow& w =
      shooters_edge::profiles::kClassicWeapons[static_cast<size_t>(n - 1)];
  if (w.fire == shooters_edge::profiles::Fire::spike) return 0.2;
  if (w.fire == shooters_edge::profiles::Fire::beam) return 0.6 + 0.2;
  return w.refire;
}

}  // namespace

int32_t fire_sound_weapon(std::string_view sample) {
  for (const shooters_edge::profiles::WeaponRow& w : shooters_edge::profiles::kClassicWeapons)
    if (sample == w.fire_sound) return w.slot;
  if (sample == "weapons/lstart.wav") return 8;  // the arc's start, where a game plays one
  return 0;
}

PackGuess pack_guess(const ShotsHeard& h, MapSense level, double t) {
  PackGuess g;
  const auto heard = [&](int32_t n) { return t - h.at[n] <= kShotMemory_s; };
  g.rockets = heard(7) || heard(6);
  if (h.last <= 0 || t - h.last_t > kShotMemory_s) return g;  // nothing heard: ammunition alone
  // The blade and the scattergun never go in a pack: the last heard of them says nothing more.
  const int32_t last = h.last >= 3 ? h.last : 0;
  switch (level) {
    case MapSense::novice:
      g.weapon = heard(7) ? 7 : last;  // a rocket heard: a launcher, whatever came after
      break;
    case MapSense::average:
      g.weapon = last;
      break;
    case MapSense::expert: {
      g.weapon = last;
      // Inside the refire no switch has gone through yet: the last weapon is still in his hand.
      g.sure = t - h.last_t <= switch_window(h.last) + kSwitchSlack_s;
      break;
    }
  }
  return g;
}

void packs_hear(Fighter& f, const server::RulesCommon& s, int me, double t) {
  const sh::Vec3f self = s.world.entities[static_cast<size_t>(me)].origin;
  const sh::Vec3f eye{self.x, self.y, self.z + f.bot.senses.set.view_height};
  // Every command's sounds (a think's hearing samples fewer): a shot is loud and plain.
  for (const sh::ViewSound& snd : f.bot.view.sounds) {
    const std::string_view sample(snd.sample);
    const bool player = snd.ent >= 1 && static_cast<size_t>(snd.ent) < f.fired.size();
    const auto e = static_cast<size_t>(snd.ent);
    if (const int32_t n = fire_sound_weapon(sample); n > 0 && player) {
      ShotsHeard& h = f.fired[e];
      h.at[n] = t;
      h.last = n;
      h.last_t = t;
      if (snd.ent == me) {  // its own: from its eye along the view its last command carried
        f.shot_from = eye;
        f.shot_dir = sh::angle_vectors({f.view_sent[0], f.view_sent[1], 0}).forward;
        f.shot_t = t;
        f.shot_number = n;
      }
    } else if (player && sample.rfind("player/", 0) == 0) {
      f.cried_at[e] = snd.origin;  // a cry, a jump, a splash: he is there
      f.cried_t[e] = t;
    } else if (sample == "weapons/r_exp3.wav" && (f.shot_number == 6 || f.shot_number == 7) &&
               t - f.shot_t < kShotLand_s && f.blast_t < f.shot_t) {
      // A blast near its own last shot's line (ahead of it): where its missile went off.
      const sh::Vec3f d = snd.origin - f.shot_from;
      const float along = d.x * f.shot_dir.x + d.y * f.shot_dir.y + d.z * f.shot_dir.z;
      const sh::Vec3f off = d - f.shot_dir * along;
      if (along > 0 && dist(off, {}) < (f.shot_number == 6 ? kOnArc : kOnLine)) {
        f.blast_at = snd.origin;
        f.blast_t = t;
      }
    }
  }
  // A pack call pending (packs_obituary): said once it may speak.
  if (t < f.pack_call_until) {
    Said said;
    said.kind = SayKind::pack;
    said.item = f.pack_call_item;
    if (mind_say(f, t, said, f.pack_call_at)) {
      f.stats.packs_called++;
      f.pack_call_until = -1e9;
    }
  }
  // Where each one in sight stands: where his pack falls if he dies (the senses drop the dead).
  for (const sh::Contact& c : f.bot.senses.contacts)
    if (c.visible && c.ent >= 1 && static_cast<size_t>(c.ent) < f.last_seen_t.size()) {
      f.last_seen_at[static_cast<size_t>(c.ent)] = at_of(c.seen);
      f.last_seen_t[static_cast<size_t>(c.ent)] = t;
    }
  if (!f.features[sh::Feature::packinfer]) return;
  // The packs it knows of: let go when old or reached; tied to the pack seen near each; a pack
  // seen lying that it did not know of is known now; a spot in sight with no pack: gone.
  std::erase_if(f.known_packs, [&](const KnownPack& p) {
    if (t - p.t > kPackKeep_s) return true;
    if (dist(self, p.at) < kPackReached) {
      f.stats.pack_reached++;
      return true;
    }
    return false;
  });
  std::vector<uint8_t> seen(f.known_packs.size(), 0);
  for (const sh::ViewEntity& v : f.bot.view.entities) {
    if (!is_backpack(s, v)) continue;
    KnownPack* near = nullptr;
    float best = kPackSame;
    for (size_t k = 0; k < f.known_packs.size(); k++) {
      KnownPack& p = f.known_packs[k];
      const float d = p.ent == v.ent ? 0.f : dist(p.at, v.origin);
      if (d < best && (p.ent < 0 || p.ent == v.ent)) {
        best = d;
        near = &p;
      }
    }
    if (near != nullptr) {
      near->ent = v.ent;
      near->at = v.origin;
      seen[static_cast<size_t>(near - f.known_packs.data())] = 1;
    } else if (dist(self, v.origin) < kPackGoRockets && sees_spot(f, s, me, eye, v.origin)) {
      KnownPack p;
      p.at = v.origin;
      p.t = t;
      p.ent = v.ent;
      p.how = static_cast<uint8_t>(PackFrom::lying);
      p.wanted = pack_draw(t, v.ent, f.slot) < kPackBother;
      f.known_packs.push_back(p);
      seen.push_back(1);
      f.stats.packs_known[static_cast<int>(PackFrom::lying)]++;
    }
  }
  for (size_t k = f.known_packs.size(); k-- > 0;) {
    const KnownPack& p = f.known_packs[k];
    if (seen[k] || t - p.t < kLanded_s || dist(self, p.at) > kSeenGone ||
        !sees_spot(f, s, me, eye, p.at))
      continue;
    f.stats.pack_seen_gone++;
    f.known_packs.erase(f.known_packs.begin() + static_cast<std::ptrdiff_t>(k));
  }
}

void packs_obituary(Fighter& f, const server::RulesCommon& s, const std::string& line, double t) {
  const bool infer = f.features[sh::Feature::packinfer];
  if (f.name.empty() || (!infer && (!f.team || !f.features[sh::Feature::packs]))) return;
  // The victim: the player whose name the line starts with and a death's words follow.
  for (size_t i = 0; i < std::size(s.world.clients); i++) {
    const std::string& name = s.world.clients[i].name;
    if (name.empty() || name == f.name || line.size() <= name.size() + 1 ||
        line.compare(0, name.size(), name) != 0 || line[name.size()] != ' ')
      continue;
    const std::string_view rest = std::string_view(line).substr(name.size() + 1);
    if (!death_words(rest)) return;
    // The kill command drops nothing.
    if (rest.rfind("becomes bored", 0) == 0 || rest.rfind("suicides", 0) == 0) return;
    const int ent = static_cast<int>(i) + 1;
    if (static_cast<size_t>(ent) >= f.fired.size()) return;
    const auto e = static_cast<size_t>(ent);
    bool mate = false;
    for (const Mate& m : f.mind.mates) mate = mate || m.name == name;
    const bool mine = line.find(f.name, name.size() + 1) != std::string::npos;
    const PackGuess g = pack_guess(f.fired[e], f.hearing, t);
    // Where it lies: seen fall; else his last cry; else, its own kill, where its shot landed.
    sh::Vec3f at;
    int how = -1;
    if (t - f.last_seen_t[e] <= kPackSeen_s) {
      at = f.last_seen_at[e];
      how = static_cast<int>(PackFrom::seen);
    } else if (t - f.cried_t[e] <= kCryFresh_s) {
      at = f.cried_at[e];
      how = static_cast<int>(PackFrom::cry);
    } else if (mine && t - f.shot_t <= kShotLand_s) {
      if (f.blast_t >= f.shot_t && t - f.blast_t <= kBlastFresh_s) {
        at = f.blast_at;
        how = static_cast<int>(PackFrom::blast);
      } else {
        for (const sh::Contact& c : f.bot.senses.contacts)
          if (c.ent == ent && t - c.seen.at < kShotLand_s + kShotMemory_s) {
            at = at_of(c.seen);
            how = static_cast<int>(PackFrom::aim);
          }
      }
    }
    if (infer && how >= 0) {
      KnownPack p;
      p.at = at;
      p.t = t;
      p.victim = ent;
      p.weapon = g.weapon;
      p.sure = g.sure;
      p.rockets = g.rockets;
      p.how = static_cast<uint8_t>(how);
      p.wanted = g.rockets || g.weapon >= 6 || pack_draw(t, ent, f.slot) < kPackBother;
      f.known_packs.push_back(p);
      f.stats.packs_known[how]++;
      f.stats.pack_guesses++;
      if (g.weapon == 7) f.stats.pack_guess_rl++;
    }
    // THE CALL: its own kill of an enemy whose pack holds a launcher or the arc, to its team.
    if (!f.team || !f.features[sh::Feature::packs] || !mine || mate) return;
    std::string item;
    if (infer) {
      item = g.weapon == 7 ? "rl" : g.weapon == 8 ? "lg" : "";
    } else if (t - f.fired[e].at[7] <= kRlHeard_s) {
      item = "rl";
    }
    if (item.empty()) return;
    if (!infer && t - f.last_seen_t[e] > kPackSeen_s) return;  // not seen fall
    if (infer && how < 0) return;                              // nowhere it could say
    // Said at its next chance to speak (a line a few seconds apart at most): pending till then.
    f.pack_call_at = infer ? at : f.last_seen_at[e];
    f.pack_call_item = item;
    f.pack_call_until = t + kPackCall_s;
    return;
  }
}

void packs_truth(Fighter& f, const server::RulesCommon& truth) {
  for (KnownPack& p : f.known_packs) {
    if (p.truth >= 0 || p.victim < 1 ||
        p.victim > static_cast<int32_t>(std::size(truth.world.clients)))
      continue;
    const auto bit =
        static_cast<int32_t>(truth.world.clients[static_cast<size_t>(p.victim - 1)].numbers.weapon);
    const int32_t n = number_of_bit(bit);
    p.truth = n >= 3 ? n : 0;  // the blade and the scattergun stay with the dead
    if (p.truth == p.weapon) f.stats.pack_guess_right++;
    if (p.truth == 7) f.stats.pack_truth_rl++;
  }
}

int32_t packs_known_goal(Fighter& f, sh::Vec3f me, const Stats& st, double t, bool /*calm*/,
                         bool in_sight) {
  if (f.known_packs.empty() || !f.bot.pilot.pilot.g || in_sight) return -1;
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  bool full = true;  // every ammunition at its most: a pack gives nothing
  for (int i = 0; i < st.arsenal->ammo_count; i++)
    full = full && st.ammo[i] >= st.arsenal->ammo_max[i];
  const bool greedy = f.sense == MapSense::novice;
  const KnownPack* best = nullptr;
  int32_t best_node = -1;
  float best_cost = 1e30f;
  const Mind& mn = f.mind;
  const bool claimed =
      mn.pack_claim_t > mn.pack_t && mn.pack_answered != mn.pack_t && t - mn.pack_t < kPackFresh_s;
  for (const KnownPack& p : f.known_packs) {
    if (claimed && dist(p.at, mn.pack_at) < kPackSame) continue;  // a mate said he takes it
    // A fight with rockets in it: the launcher's rockets (the launcher too, likely) are worth
    // going far for, calm or not; any other is left to `pack_near`.
    // A novice wants everything: every pack he knows of is worth the way, full or not.
    const bool big = p.rockets || p.weapon >= 6 || greedy;
    // A lesser pack is left to the one in sight (`pack_near`, ammunition short): going out of the
    // way for one when calm cost a head-to-head set about 35 frags (e1m2, twelve seeds).
    if (!big) continue;
    if (!greedy && full && (p.weapon == 0 || (st.items & (1 << (p.weapon - 2))) != 0))
      continue;  // nothing in it it lacks
    const float d = dist(me, p.at);
    if (d >= (big ? kPackGoRockets : kPackGo)) continue;
    const int32_t node = sh::nav_nearest(g, p.at);
    if (node < 0 || (f.reach_at >= 0 && !f.reach.reached(node))) continue;
    const float cost = big ? d * 0.4f : d;
    if (cost < best_cost) {
      best_cost = cost;
      best = &p;
      best_node = node;
    }
  }
  if (best == nullptr) return -1;
  if (best->t != f.pack_going) {
    f.pack_going = best->t;
    f.stats.pack_trips++;
  }
  return best_node;
}

int32_t packs_goal(Fighter& f, sh::Vec3f me, const Stats& st, double t) {
  Mind& mn = f.mind;
  if (t - mn.pack_t > kPackFresh_s || !f.bot.pilot.pilot.g) return -1;
  if (dist(me, mn.pack_at) < kPackReached) {
    mn.pack_t = -1e9;  // had, or gone
    return -1;
  }
  // A mate answered the call (`coming [ga]`) before this player did: his pack.
  if (mn.pack_claim_t > mn.pack_t && mn.pack_answered != mn.pack_t) return -1;
  const bool lack = !has_launcher(st);
  if (mn.pack_rl ? !lack : false) return -1;  // a launcher of its own: the pack is a mate's
  // The quad's carrier goes; else only the one who believes himself the nearest of his team.
  const bool quad = (st.items & 4194304) != 0;
  if (!quad) {
    const float mine = dist(me, mn.pack_at);
    for (const Mate& m : mn.mates)
      if (t - m.at_t < 5 && dist(m.at, mn.pack_at) < mine && !(m.has_rl && t - m.weapon_t < 20))
        return -1;
  }
  // Its answer, as a coming bind says it: `coming` and where it is now, so the rest let it be.
  if (mn.pack_answered != mn.pack_t) {
    Said said;
    said.kind = SayKind::coming;
    if (mind_say(f, t, said, me)) mn.pack_answered = mn.pack_t;
  }
  f.stats.packs_fetched++;
  return sh::nav_nearest(*f.bot.pilot.pilot.g, mn.pack_at);
}

bool packs_bore(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
                sh::MoveCmd& cmd) {
  const sh::Vec3f at = s.world.entities[static_cast<size_t>(me)].origin;
  if (t >= f.bore_until) {
    if (!f.team || f.teamwork == MapSense::novice || sh::selfish(f.traits)) return false;
    if (!has_launcher(st) || launcher_rounds(st) < 1 || t - f.bored_at < kBoreGap_s) return false;
    if (st.health + st.armor * st.armortype > kBoreWeak) return false;  // its rocket would not do
    bool carrier = false;
    for (const Mate& m : f.mind.mates)
      carrier = carrier || (m.quad && t - m.quad_t < 25 && t - m.at_t < 5 &&
                            dist(m.at, at) < kBoreNear && !(m.has_rl && t - m.weapon_t < 20));
    if (!carrier) return false;
    Said said;
    said.kind = SayKind::pack;
    said.item = "rl";
    mind_say(f, t, said, at);
    f.bore_until = t + kBoreHold_s;
    f.bored_at = t;
    f.stats.bored++;
  }
  // The launcher in hand, the view at its own feet, still; fired once it is up.
  if (!holds_launcher(st)) {
    cmd.impulse = static_cast<uint8_t>(launcher_number(*st.arsenal));
  } else {
    cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
  }
  cmd.angles.x = sh::quantize_angle(89);
  cmd.forward = cmd.side = 0;
  f.bot.set_by = "bore";
  return true;
}

}  // namespace shooters_edge::bots
