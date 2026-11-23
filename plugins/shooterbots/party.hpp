// A PARTY OF BOTS PLAYING ONE CAMPAIGN MAP TOGETHER: each bot runs its own way through the map
// (way_through.hpp, the 100% run with `complete`), and the party splits the work between them, as
// people playing co-op do ("you take the east wing, I'll take the cellar"):
// - WHO TAKES WHAT: every stop open (a step: a button, a key, a trigger, a secret; a monster
//   alive) belongs to the bot of the party nearest to it BY THE WALK (each bot's own search from
//   its last plan, `WayThrough::search`: a stop behind a wall goes to the bot who reaches it
//   first, not to the one on the other side; straight distance only where no bot's search has a
//   time for it), or to the one already on it
//   (`WayThrough::step`, `WayThrough::prey`), so the map splits by who is closest and nobody walks
//   across it for what a mate is about to do. What is a mate's is written into each bot's
//   `mates_steps` and `mates_prey`, which his plan skips while he has work of his own; once he has
//   none he helps (`WayThrough::mates_help`), and the exit waits until nothing is left for any of
//   them (the first through it ends the level for all).
// - WHAT ONE KNOWS, ALL KNOW: a place one has seen is seen by all (`WayThrough::seen`), and a
//   secret or a trigger one has walked through is done for all (`WayStep::ever`); a button, a key
//   door and a monster are read from the world by each already.
// - STAYING WITH THE PEOPLE (`PartyOptions::leash`, the humans' places): with people playing, a
//   stop further than the leash from every one of them is no bot's, so the bots work around the
//   people and never run the level out from under them; the exit is the people's
//   (`PartyOptions::exit_held`: no bot plans it, `exits_keep_off` turns a step into one back),
//   and a bot whose goal lies beyond the leash (nothing near the people left: the map's far
//   places) goes back to the nearest person instead (`party_stay`). The escort (companion.hpp)
//   is the other half of supporting a person: a companion keeps at his shoulder; a party member
//   (a "helper" in a hosted co-op game, `PartyWork`) works the level near him.
//
// The same for every game on the shooter kit (Quake's episodes, Quake II's units): it reads only
// the bots' own plans and the places the server sends. Deterministic: the bots in their order,
// ties to the first.
//
// Off the hot path: a few hundred distances per bot, at each plan.
//
// ge:summary A party of bots splitting a campaign map's work: nearest takes it, shared knowledge
// ge:keywords party co-op coop split share route mates mates_steps mates_prey mates_help leash
// ge:keywords humans support campaign 100-percent party_share divide-work walk route-cost
// ge:keywords helper hosted party_stay party_work exit_held
// ge:skill shooterbots bots
// ge:test tests/shooterbots_party_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "shooter/map_spec.hpp"
#include "shooter/usercmd.hpp"
#include "shooter/vec.hpp"
#include "shooterbots/bot_view.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/way_through.hpp"

namespace ge::shooterbots {

constexpr float kPartyKeep = 64;  // a stop a bot is nearer by less than this stays its holder's
constexpr double kPartyKeep_s = 0.25;  // the same by the walk, s
constexpr float kPartyRun = 320;       // units a second: a straight distance as a walk's time
constexpr float kPartyLeash = 768;     // a hosted co-op game's leash round the people, units
constexpr float kPartyNear = 160;      // a bot sent back to the people stops this near one
constexpr float kPartyClear = 72;      // nearer a person than this a bot steps away from him

struct PartyMember {
  WayThrough* way = nullptr;
  int ent = 0;        // its entity (a client's: 1 for the first)
  bool alive = true;  // a dead one holds nothing until he is back
};

struct PartyOptions {
  float leash = 0;                // (people playing) a stop further than this from all is none's
  std::vector<sh::Vec3f> humans;  // the people's places
  std::vector<float> facing;      // (with `humans`) where each looks, degrees of yaw
  bool exit_held = false;         // (people playing) the exit is theirs: no bot's, ever
};

struct PartyStats {
  int64_t shares = 0;     // times the work was split
  int64_t seen_told = 0;  // places one saw told to the others
  int64_t ever_told = 0;  // secrets and triggers one did told to the others
  int64_t leashed = 0;    // stops left to no one for being too far from the people (last share)
  int64_t walked = 0;     // stops given by the walk's time rather than straight distance (last)
};

// What the party remembers between shares: each stop's holder (a member's index, -1 none).
struct Party {
  std::vector<int8_t> step_holder, prey_holder;
  PartyStats stats;
};

// The work split among `party` (each `way` already made by way_init on the same map, so their
// steps match index for index), and what one knows told to all. Called before each bot plans.
void party_share(Party& p, std::span<PartyMember> party, const MapNav& nav, const BotView& s,
                 const PartyOptions& opt);

// After `way_plan`, with people playing: a goal further than the leash from every person (or
// none) traded for a node kPartyNear behind where the nearest person looks (`facing`; without it,
// short of him on this side): never onto him or at his elbow, where in a corridor or a doorway (a
// door a player stands in turns back) the person's way is blocked.
// True when it sent him back.
bool party_stay(Fighter& f, const MapNav& nav, const BotView& s, int me, const PartyOptions& opt);

// A PARTY MEMBER BESIDE PEOPLE (a "helper" in a hosted co-op game): a fighter playing the map's
// way through (`complete`) as one of a party leashed to the people, with what the way bot's
// command needs after its own.
struct PartyWork {
  WayThrough way;
  Wedge wedge;
  int64_t stays = 0;         // plans he went back to the people
  int64_t exits_turned = 0;  // commands turned back from an exit
  int64_t gave_way = 0;      // commands he stepped away from a person
};
// The fighter `f` (set up by `fighter_init` on `nav`) made a party member for `spec`'s map.
void party_work_init(PartyWork& w, Fighter& f, const MapNav& nav, const BotView& s,
                     const sh::MapSpec& spec);
// His plan (entity `me`, at time `t`): the way's, then kept to the people.
void party_work_plan(PartyWork& w, Fighter& f, const MapNav& nav, const BotView& s, int me,
                     double t, const PartyOptions& opt);
// His command after the fighter's own: the water's way, a trigger walked into, a wedge shoved
// free, out of a person's way, and never into an exit.
void party_work_command(PartyWork& w, Fighter& f, const MapNav& nav, const BotView& s, int me,
                        double t, const PartyOptions& opt, sh::MoveCmd& cmd);
// A bot (entity `me`) nearer one of `humans` than kPartyClear (and on his floor) steps straight
// away from him: in a corridor or a doorway a bot standing by a person is in his way, and a bot
// that waits there blocks him for good. True when it turned the keys.
bool party_give_way(const BotView& s, int me, const std::vector<sh::Vec3f>& humans,
                    sh::MoveCmd& cmd);

}  // namespace ge::shooterbots
