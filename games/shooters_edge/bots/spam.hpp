// KEEPING AN AREA SAFE: an expert holding a key area with its item gone and not due for a while
// steps out to a spot near it and fires rockets at the places enemies come in by, the teleporters'
// exits and the powers' spawns, so whoever comes out there meets a rocket before he reaches the
// area; he is back on his area before its item is due.
//
// Only with rockets to spare (`kSpamRockets` or more, the launcher held or carried), only with no
// one in sight, never at a spot a teammate is believed near, a rocket every `kSpamGap_s`.
//
// The SPOTS are read off the map once (`spam_init`); the place each is fired from is found once a
// claim, among the nodes within the holder's leash of his area that see the spot from far enough
// for a rocket to arm and near enough to land (`spam_plan`).
//
// SPAM AT NEWS: anyone with the launcher and rockets to spare (`kNewsRocketsMost` less its
// aggression trait, 10 for the most aggressive to 19 for the least), nobody in sight, fires at the
// outer end of a way into the area he stands in (the map plan's entries) that leads from somewhere
// unsafe: an enemy seen, heard or called out within `kNewsNear` beyond it in the last
// `kNews_s`, or where a teammate died lately. The view turns there on the bot's hands (its `look`),
// the rocket goes once the aim is on it, at most `kNewsShots` a piece of news, one every
// `kSpamGap_s`; never at a spot a teammate is near or with one in the line, never nearer than
// `kSpamNear` (its own blast).
//
// Deterministic: pure functions of the fighter's beliefs, the map and the server's state.
//
// ge:summary An expert holder fires rockets at teleporter exits and power spawns near his area
// ge:keywords spam rocket-spam area-control teleport-exit power-spawn hold leash spam_plan
// ge:keywords spam_fire spam_init spam-at-news unsafe-area ways-in rockets-to-spare aggression
// ge:keywords spam_news_look spam_news_fire
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr float kSpamRockets = 20;  // rockets carried before any is spent on a spot
constexpr double kSpamGap_s = 1.5;  // a rocket at a spot this often
constexpr double kSpamBack_s = 5;   // back on the area this long before its item is due
constexpr float kSpamNear = 250, kSpamFar = 1400;  // a spot fired at from this far, units
constexpr float kNewsRocketsMost = 20;  // rockets to spare at news: this less the aggression trait
constexpr double kNews_s = 8;           // news of an enemy this old still makes a way in unsafe
constexpr float kNewsNear = 1000;       // and this near beyond it
constexpr int kNewsShots = 3;           // rockets a piece of news is worth

// The map's spots worth a rocket (teleporters' exits, the powers' spawns), off its entity text.
void spam_init(Fighter& f, const char* entities);

// Whether the holder should be out firing at a spot now; sets `spam_node` (where it fires from)
// and `spam_spot` (what at) when so, and clears them when not.
bool spam_plan(Fighter& f, const server::RulesCommon& s, const Stats& st, double t);

// The rocket itself, when the holder stands where it fires from with no one in sight: the view on
// the spot and the trigger, into `cmd`. True when it fired.
bool spam_fire(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
               ge::shooter::MoveCmd& cmd);
// Spam at news, before the command is made: the way in to fire at turned to (`f.bot.look`), or
// nothing. After it is made, `spam_news_fire` presses the trigger once the view is on it.
void spam_news_look(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t);
bool spam_news_fire(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
                    ge::shooter::MoveCmd& cmd);

}  // namespace shooters_edge::bots
