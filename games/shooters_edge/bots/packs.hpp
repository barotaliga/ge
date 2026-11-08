// LAUNCHERS IN BACKPACKS: a dead player drops what he held, so a rocket launcher changes hands
// through packs, and a team that plays well moves its launchers to whoever needs one most.
// - THE CALL (`packs_obituary`): a player who killed an enemy he heard fire a rocket within
//   `kRlHeard_s` says where the pack lies (`rl pack [mh]`), as people do, so a mate without a
//   launcher comes for it;
// - THE FETCH (`packs_goal`): a pack called within `kPackFresh_s` is gone for by the mate who
//   needs it (no launcher of his own) and believes himself the nearest of his team to it, the
//   quad's carrier first;
// - THE BORE (`packs_bore`): a good teammate (not selfish, not a novice) holding the launcher,
//   weak enough that his own rocket kills him, with a mate on the quad near who has none, says
//   `rl pack` where he stands and fires at his own feet: his launcher is the quad's.
//
// ge:summary Launchers in backpacks: an enemy's pack called, fetched by who needs it, the bore
// ge:keywords backpack pack rl-pack launcher bore suicide drop-rl quad fetch call teamplay
// ge:keywords packs_obituary packs_goal packs_bore packs_hear
// ge:skill bots
// ge:test tests/shooters_edge_talk_test.cpp
#pragma once

#include <string>

#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr double kRlHeard_s = 4;     // an enemy heard firing a rocket this lately had a launcher
constexpr double kPackFresh_s = 12;  // a called pack is gone for within this
constexpr float kBoreNear = 600;     // the quad's carrier this near, units
constexpr float kBoreWeak = 75;      // health and armour's protection at most, for one rocket
constexpr double kBoreGap_s = 30;    // a bore at most this often

// The rocket launches the last command's sounds carried, by the entity that fired (every command).
void packs_hear(Fighter& f, double t);
// A server line read: a kill of an enemy heard firing rockets called as a pack where he fell.
void packs_obituary(Fighter& f, const server::RulesCommon& s, const std::string& line, double t);
// The node to go to for a called pack this player should fetch, or -1.
int32_t packs_goal(Fighter& f, ge::shooter::Vec3f me, const Stats& st, double t);
// The bore on `cmd` when its time has come (above); true while it holds the command.
bool packs_bore(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
                ge::shooter::MoveCmd& cmd);

}  // namespace shooters_edge::bots
