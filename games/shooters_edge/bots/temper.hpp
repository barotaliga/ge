// A FIGHTER'S TEMPERAMENT AT WORK (plugins/shooter/temper.hpp decides): its own might off what it
// carries, and each enemy's as far as it can tell it.
//
// An enemy is believed a fresh spawn (100 health, a little more than the scattergun) until he is
// heard: the big armour or the mega taken add to his body, the weapon heard fired or picked up
// sets his arms, the quad triples them for its time, each cry of pain takes some off, a death puts
// him back to a fresh spawn. What is believed fades back to the fresh spawn when nothing is heard
// for `kFoeFade_s`. A novice reads none of it: to him every enemy is the fresh spawn.
//
// ge:summary A fighter's temperament at work: its own might, each enemy's believed off sounds
// ge:keywords own_might foe_heard foe_might foe-read outmatched temper fresh-spawn
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include <string>

#include "shooter/hearing.hpp"
#include "shooter/temper.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr double kFoeFade_s = 30;  // an enemy's read fades back to a fresh spawn after this

ge::shooter::Might own_might(const Stats& st);
// What a sound `h` from an enemy tells this fighter of him.
void foe_heard(Fighter& f, const ge::shooter::HeardSound& h, double t);
// (`ledgerforget`, M18r.md 0q) In a team game an enemy neither seen nor heard for this long is
// forgotten: his body believed the typical live enemy's again (se-team `team.ledger.truth-mean`
// on dm2: 115), not the last count (which only ever fell, by his cries).
constexpr double kForget_s = 3;
// (`spawnkept`) a fresh spawn read off a death (heard, or the kill feed's) is kept this long out of
// sight: a respawned enemy stays near his 100 longer than a fight's read does
constexpr double kSpawnKept_s = 10;
constexpr float kFoeTypical = 115;
// (`ledgerhits`, M18r.md 0q) Its own rockets counted: one that bursts within the blast's reach of
// where an enemy seen within kHitSeen_s is believed takes `kRocketSplash` less half the distance
// (kRocketDirect on him), the armour's share off (kArmourShare); his cry right after not again.
constexpr float kRocketSplash = 120, kRocketDirect = 110, kDirectReach = 36;
constexpr float kArmourShare = 0.8f;
constexpr double kHitSeen_s = 0.5, kCryAfterHit_s = 0.3;
// (`ledgerheal`) a health box heard taken heals about this (the small 15, the big 25), and no
// further than this body (100 health and an armour's share)
constexpr float kBoxHeals = 20, kHealedUnder = 150;
// (`ledgerobits`, M18r.md 0q) A line its client was printed: an enemy's obituary (his name first,
// a death's words after) sets his line to a fresh spawn (100, the plain weapon): the kill feed a
// person reads, every death on the map, not only the cries in earshot.
void ledger_obituary(Fighter& f, const ge::shooter::BotView& s, const std::string& line, double t);
// Each command, after its ammunition is read: `fired` a rocket of its own left this command.
void ledger_own_shots(Fighter& f, const ge::shooter::BotView& s, int me, bool fired, double t);
// The body (health and the armour's share) this fighter believes the enemy `ent` has at `t`.
float foe_body(const Fighter& f, int ent, double t);
// Each command: the enemies in its sight noted as seen (`ledgerforget`).
void foes_seen(Fighter& f, double t);
// The might this fighter believes the enemy `ent` has at `t`.
float foe_might(const Fighter& f, int ent, double t);
// The node of the nearest teammate whose place is fresh and who is not at hand, or -1.
int32_t nearest_mate_node(const Fighter& f, ge::shooter::Vec3f at, double t);

}  // namespace shooters_edge::bots
