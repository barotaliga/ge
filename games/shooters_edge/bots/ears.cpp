// What a fighter hears (ears.hpp): the mod's samples, a pickup put on its item, a spawn on its
// point.
//
// ge:summary What a fighter hears: the sample table, pickups snapped to items, spawns to points
// ge:keywords ears fighter_hear sound_table hears_as snap spawn-point
// ge:skill shooter bots
// ge:test tests/shooters_edge_ears_test.cpp
#include "shooters_edge/bots/ears.hpp"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "progs/entity_text.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/temper.hpp"
#include "shooters_edge/profiles/items.hpp"

namespace shooters_edge::bots {

namespace pf = shooters_edge::profiles;

namespace {

namespace sh = ge::shooter;
namespace pg = ge::progs;
using sh::SoundClass;

constexpr float kSnapMin = 200;     // a guess is put on a spot this near it at the least, units
constexpr float kSnapSlack = 1.5f;  // and up to this many times the ear's error further

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

}  // namespace

std::span<const sh::SoundMeaning> sound_table() { return pf::kClassicTables.sounds; }

const char* hears_as(const ItemSpot& it) {
  const pf::ItemWords* w = pf::item_words(pf::kClassicTables.words, it.kind, it.weapon);
  return w != nullptr ? w->heard.data() : "";
}

void ears_init(Fighter& f, sh::AimSkill skill, const char* entities) {
  f.ears = sh::hear_skill_of(skill);
  f.bot.senses.set.hear_players = false;  // its own ears decide what it hears
  f.spawns.clear();
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  if (!entities || !pg::parse_entity_text(entities, blocks, error)) return;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    if (!cn || std::strcmp(cn, "info_player_deathmatch") != 0) continue;
    sh::Vec3f at;
    if (parse_vec(pg::block_value(b, "origin"), at)) f.spawns.push_back(at);
  }
}

EarNews fighter_hear(Fighter& f, const server::QcServer& s, int me, sh::Vec3f ear, double t,
                     bool fighting) {
  EarNews news;
  if (t == f.heard_t) return news;
  f.heard_t = t;
  sh::hear(f.bot.view, me, ear, f.ears, pf::kClassicTables.sounds, fighting, f.rng, f.heard,
           &f.hear_stats);
  const bool knows_map = f.sense != MapSense::novice;
  const int players = s.sv.set.maxclients;
  for (const sh::HeardSound& h : f.heard) {
    if (h.ent < 1 || h.ent > players) continue;  // a missile, a door, a lift: no one is there
    news.heard++;
    sh::Vec3f place = h.guess;
    const float snap = kSnapMin + kSnapSlack * f.ears.range_error * h.dist;
    // Only a UNIQUE pickup says which item went: an ammunition box's sound is a backpack's too,
    // and a health's is any of several, so a common one places the player and no more.
    if (knows_map && h.meaning && h.meaning->unique &&
        (h.cls == SoundClass::pickup || h.cls == SoundClass::power)) {
      // The item of that pickup nearest the guess, believed there until now.
      int32_t pick = -1;
      float best = snap;
      for (size_t i = 0; i < f.items.size(); i++) {
        const ItemSpot& it = f.items[i];
        if (!it.present || !item_in_mind(f, static_cast<int32_t>(i)) ||
            h.meaning->word != hears_as(it))
          continue;
        const float d = dist(it.at, h.guess);
        if (d < best) {
          best = d;
          pick = static_cast<int32_t>(i);
        }
      }
      if (pick >= 0) {
        ItemSpot& it = f.items[static_cast<size_t>(pick)];
        place = it.at;
        const bool times =
            f.sense == MapSense::expert || (f.sense == MapSense::average && !key_item(it));
        it.present = it.confirmed = false;
        it.timed = times;
        it.back_at = t + it.respawn_s * (it.timed ? 1.f : kGuessLate);
        f.stats.taken++;
        f.stats.heard_taken++;
        mega_taken(f, pick, t, false);
        news.taken = pick;
      }
    } else if (knows_map && h.cls == SoundClass::spawn && !f.spawns.empty()) {
      float best = snap;
      for (const sh::Vec3f& sp : f.spawns) {
        const float d = dist(sp, h.guess);
        if (d < best) {
          best = d;
          place = sp;
        }
      }
      if (best < snap) {
        news.spawns++;
        f.stats.heard_spawns++;
      }
    }
    const int slot = h.ent - 1;
    const bool mate = (f.bot.senses.friends >> slot & 1u) != 0;
    if (mate) {
      for (Mate& m : f.mind.mates)
        if (m.slot == slot && t - m.at_t > 0) {
          m.at = place;
          m.at_t = t;
        }
    } else {
      foe_heard(f, h, t);  // what he took, fired or cried tells his might (temper.hpp)
      sh::sense_heard(f.bot.senses, h.ent, me, place, t, f.why);
    }
  }
  return news;
}

}  // namespace shooters_edge::bots
