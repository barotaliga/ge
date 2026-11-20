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
constexpr float kFoeTypical = 115;
// The body (health and the armour's share) this fighter believes the enemy `ent` has at `t`.
float foe_body(const Fighter& f, int ent, double t);
// Each command: the enemies in its sight noted as seen (`ledgerforget`).
void foes_seen(Fighter& f, double t);
// The might this fighter believes the enemy `ent` has at `t`.
float foe_might(const Fighter& f, int ent, double t);
// The node of the nearest teammate whose place is fresh and who is not at hand, or -1.
int32_t nearest_mate_node(const Fighter& f, ge::shooter::Vec3f at, double t);

}  // namespace shooters_edge::bots
