// LAUNCHERS IN BACKPACKS: a dead player drops what he held, so a rocket launcher changes hands
// through packs, and a team that plays well moves its launchers to whoever needs one most.
// - THE CALL (`packs_obituary`): a player who killed an enemy he heard fire a rocket within
//   `kRlHeard_s` says where the pack lies (`rl pack [mh]`), as people do, so a mate without a
//   launcher comes for it;
// - THE FETCH (`packs_goal`): a pack called within `kPackFresh_s` is gone for by the mate who
//   needs it (no launcher of his own) and believes himself the nearest of his team to it, the
//   quad's carrier first; he answers `coming [ga]` (his own place, as a coming bind says it), and
//   a mate who hears a bare `coming` right after the call lets the pack be;
// - THE BORE (`packs_bore`): a good teammate (not selfish, not a novice) holding the launcher,
//   weak enough that his own rocket kills him, with a mate on the quad near who has none, says
//   `rl pack` where he stands and fires at his own feet: his launcher is the quad's.
//
// EVERY DEATH DROPS A PACK (`packinfer`): a dying player drops his ammunition and the weapon in
// his hand (none for the blade or the scattergun; none after the kill command). A player knows
// so, and works out two things a pack does not show:
// - WHERE IT LIES (`PackFrom`): where he saw the victim fall; else where he heard the victim's
//   last cry (pain, death, a jump: any sound of that player); else, the kill being his own,
//   where his own shot landed: the blast heard nearest his last shot's line (a rocket into the
//   water, nobody seen) or the place he aimed at;
// - WHAT IS IN IT (`pack_guess`), from the shots heard from the victim in the fight, by the
//   player's ear (`Fighter::hearing`): a NOVICE believes a launcher whenever he heard the victim
//   fire one; an AVERAGE player believes the last weapon heard; an EXPERT knows the last weapon
//   heard is in the pack for certain when the death fell inside its refire (a switch is held
//   until the weapon is ready again: people's scripts that hand back the scattergun after each
//   rocket cannot have done it yet), and believes it, unsure, after.
// A pack from a fight the victim fired rockets in is worth much more (a launcher's rockets, the
// launcher itself): gone for from far and over a role's errand (`packs_known_goal`); any other is
// gone for most often, near and when nothing else is on (no key item it believes up on its way).
// A NOVICE wants everything: every pack he sees or knows of is worth the way to him, full or not.
// Each known pack is let go when reached, seen gone, or a minute old.
//
// ge:summary Backpacks: every death's pack placed and its weapon guessed by skill; the call, the
// ge:summary fetch and the bore of launchers
// ge:keywords backpack pack rl-pack launcher bore suicide drop-rl quad fetch call teamplay
// ge:keywords packs_obituary packs_goal packs_bore packs_hear pack_guess packs_known_goal
// ge:keywords packinfer pack-guess refire weapon-switch death-cry blast-heard known-pack
// ge:skill bots
// ge:test tests/shooters_edge_packs_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr double kPackFresh_s = 12;     // a called pack is gone for within this
constexpr float kBoreNear = 600;        // the quad's carrier this near, units
constexpr float kBoreWeak = 75;         // health and armour's protection at most, for one rocket
constexpr double kBoreGap_s = 30;       // a bore at most this often
constexpr double kShotMemory_s = 10;    // shots heard from a player kept this long (the fight's)
constexpr double kPackKeep_s = 60;      // a known pack let go after this long
constexpr double kSwitchSlack_s = 0.1;  // past the refire, still "inside" it (the news's lag)
constexpr float kPackGo = 600;          // a lesser pack gone for within this, units
constexpr float kPackGoRockets = 2400;  // and one from a fight with rockets in it
constexpr double kPackBother = 0.85;    // the share of lesser packs a player bothers with

// How a known pack's place came to be known.
enum class PackFrom : uint8_t { seen, cry, blast, aim, lying };
constexpr int kPackFroms = 5;

// What a player believes is in a dead player's pack, from the shots heard from him (`h`) by
// time `t` (his death), with `level`'s reasoning (above): the weapon by number (0: ammunition
// alone), certain or not, and whether he fired rockets in the fight.
struct PackGuess {
  int32_t weapon = 0;
  bool sure = false;
  bool rockets = false;
};
PackGuess pack_guess(const ShotsHeard& h, MapSense level, double t);
// The weapon number (1..8) a fire sound says, 0 for none.
int32_t fire_sound_weapon(std::string_view sample);

// The last command's sounds and sights taken in (every command): the weapons heard fired by who
// fired, each player's cries, its own shot and its blast, the packs seen lying or seen gone.
void packs_hear(Fighter& f, const server::RulesCommon& s, int me, double t);
// A server line read: a death's pack placed and guessed (`packinfer`); a kill of its own whose
// pack holds a launcher or the arc called to the team (`packs`).
void packs_obituary(Fighter& f, const server::RulesCommon& s, const std::string& line, double t);
// The lab's truth (the runner's, never the bot's): the weapon each pack it just placed really
// holds, read off the server `truth`, its guesses counted right or wrong.
void packs_truth(Fighter& f, const server::RulesCommon& truth);
// The node of the known pack to go for now, or -1: a pack from a fight with rockets in it (any
// pack for a novice) is gone for unless an enemy is in sight; `calm` is not read now.
int32_t packs_known_goal(Fighter& f, ge::shooter::Vec3f me, const Stats& st, double t, bool calm,
                         bool in_sight);
// The node to go to for a called pack this player should fetch, or -1.
int32_t packs_goal(Fighter& f, ge::shooter::Vec3f me, const Stats& st, double t);
// The bore on `cmd` when its time has come (above); true while it holds the command.
bool packs_bore(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
                ge::shooter::MoveCmd& cmd);

}  // namespace shooters_edge::bots
