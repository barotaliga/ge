// PLAYERS VOTE ON A SERVER'S SETTINGS FOR ONE MAP: any player may call a vote to change a setting
// (`vote fraglimit 30`), everyone on the server answers `vote yes` or `vote no`, and when more than
// half of them say yes the game makes the change. It lasts the map: at the next map every change a
// vote made goes back to what it was. The same for every game on the engine (docs/servers.md §
// Votes); the game says what a setting is and makes the change (`VoteTarget`).
//
// WHO VOTES: the VOTERS are the people on the server (players and spectators, never bots), kept by
// the game with `vote_seat` as they come and go. A vote PASSES the moment its yes are more than
// half the voters, and FAILS the moment its no are half or more (yes can no longer be the
// majority), or when `time_s` (`sv_vote_time`) runs out first. The caller's own ballot is yes, so
// a player alone on a server passes a vote at once. A voter who leaves takes his ballot with him
// and the vote is weighed again among those left; when nobody is left it is dropped. One vote is
// open at a time; a player whose vote failed waits `wait_s` (`sv_vote_wait`) before calling
// another.
//
// WHAT MAY BE VOTED ON (`vote_permitted`): the setting's name against two lists of names, where `*`
// stands for any run of characters (`sv_vote*`, `*password*`). A name on the DENY list
// (`sv_vote_deny`) never may; with an empty ALLOW list (`sv_vote_allow`) every other name may,
// else only a name on it. The default deny list (`kVoteDenyDefault`) keeps the owner's settings
// out: passwords, rcon, the server's name and slots, its address, the vote's own settings, the
// mod, the seed, the rotation and the frame rate. Past the lists the game's check decides
// (`VoteTarget::check`): a setting it does not have, a value out of range, a player not there.
//
// A CHANGE is a setting's name and value. The game's check names what it changes (`key`: the
// name, or a per-player grant's name and player), the value that puts it back (`before`) and may
// rewrite the value into its own form. When a vote passes the change is DUE (`vote_take`) and kept
// for the map; a second vote on the same key keeps the first `before`. `vote_map_over` hands back
// every kept change with its `before` as the value, newest first, and the map starts clean.
//
// THE WORDS (`vote_command`, a client's command or a console line): `vote <setting> <value...>`
// calls one (quotes round the value are taken off); `vote yes`, `vote no` (and `yes`, `no` alone
// while a vote is open) answer, and may change an answer; `vote cancel` takes back the caller's
// own; `vote` alone says what is open, `vote rules` what may be voted on. Anything else is not the
// vote's (false). Every act and every refusal (with why and what to do instead) is a line in
// `said`, and beside it in `notes` what it is about and who it is for, so a game prints each to
// everyone or to the one player.
//
// A pure function of the acts in order on the server's clock: no wall clock, no randomness, and
// nothing allocated per frame but the lines said.
//
// ge:summary Votes on a server's settings for one map: call, yes and no, majority, revert, lists
// ge:keywords vote callvote vote-yes vote-no majority server-settings per-map revert allow-list
// ge:keywords deny-list whitelist blacklist sv_vote sv_vote_time sv_vote_wait sv_vote_allow
// ge:keywords sv_vote_deny vote_command vote_seat vote_take vote_map_over vote_permitted
// ge:skill net
// ge:test tests/server_vote_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::server {

constexpr int kVoteSeats = 32;

// The names no vote may change unless the owner says otherwise (`sv_vote_deny`'s fallback).
extern const char* const kVoteDenyDefault;

struct VoteSettings {
  bool enabled = true;  // sv_vote: 0 turns every vote off
  double time_s = 30;   // sv_vote_time: how long a vote stays open, s
  double wait_s = 30;   // sv_vote_wait: after a failed vote, before its caller's next
  std::string allow;    // sv_vote_allow: names that may (empty: all but `deny`)
  std::string deny = kVoteDenyDefault;  // sv_vote_deny: names that never may
};

enum class VoteWhy : uint8_t {
  ok,
  off,          // sv_vote is 0
  not_voter,    // the slot is not one of the voters (a bot, or not on the server)
  busy,         // another vote is open
  none_open,    // no vote is open to answer or cancel
  waiting,      // the caller's last vote failed less than `wait_s` ago
  denied,       // the name is on the deny list
  not_allowed,  // the allow list is set and the name is not on it
  no_value,     // `vote <setting>` with no value
  refused,      // the game's check refused it (its own words in the line)
  not_caller,   // only the caller may cancel
  same,         // that answer was given already
};
const char* vote_why_text(VoteWhy w);

// `name` against `pattern`, where `*` matches any run of characters (none too).
bool vote_pattern_match(std::string_view pattern, std::string_view name);
// Whether `name` may be voted on by the lists alone (VoteWhy::ok, denied or not_allowed). The
// lists' words are separated by spaces or commas.
VoteWhy vote_permitted(const VoteSettings& settings, std::string_view name);

// What the game's check says of a change.
struct VoteCheck {
  std::string key;     // what it changes (empty: the name)
  std::string value;   // the value in the game's own form (empty: as asked)
  std::string before;  // the value that puts it back as the map had it
  std::string error;   // when refused: why, and the next action
};

// The game's half: whether `name` may be set to `value` now (true, `out` filled) or not (false,
// `out.error` says why). The game makes a passed change itself (`vote_take`).
struct VoteTarget {
  void* ctx = nullptr;
  bool (*check)(void* ctx, std::string_view name, std::string_view value, VoteCheck& out) = nullptr;
};

// A change a vote made: due to be made (`vote_take`), or due to be put back (`vote_map_over`,
// `value` then the `before`).
struct VoteChange {
  std::string key, name, value, before;
};

enum class VoteNoteKind : uint8_t {
  called,     // `slot` called a vote
  answered,   // `slot` answered it
  passed,     // the vote passed: its change is due
  failed,     // the no won
  expired,    // the time ran out
  cancelled,  // the caller took it back, or nobody was left to vote
  reverted,   // a change went back at the map's end
  refused,    // `slot`'s act was refused (`why`)
  told,       // an answer for `slot` alone (`vote`, `vote rules`)
};
struct VoteNote {
  VoteNoteKind kind = VoteNoteKind::told;
  int slot = -1;  // who it is about
  int to = -1;    // who it is for: -1 everyone, else that slot alone
  VoteWhy why = VoteWhy::ok;
};

enum class Ballot : uint8_t { none, yes, no };

struct VoteBook {
  VoteSettings settings;
  // The voters, kept by the game (`vote_seat`): a bit a slot, and each one's name.
  uint32_t voters = 0;
  std::array<std::string, kVoteSeats> names;
  std::array<double, kVoteSeats> failed_at{};  // when the slot's last vote failed (0: never)
  // The vote open (`open`): what, who called it, when it runs out, and every slot's ballot.
  bool open = false;
  VoteChange asked;
  int caller = -1;
  double ends = 0;
  std::array<Ballot, kVoteSeats> ballots{};
  // The count when the last vote closed (passed, lost, ran out, dropped or taken back).
  int last_yes = 0, last_no = 0, last_voters = 0;
  std::vector<VoteChange> due;    // passed, for the game to make (`vote_take`)
  std::vector<VoteChange> kept;   // this map's changes, oldest first, each with its first `before`
  std::vector<std::string> said;  // lines for the game to print, in order (the game clears both)
  std::vector<VoteNote> notes;    // what each line of `said` is about and who it is for
  int called = 0, passed = 0, failed = 0;  // votes called, passed and lost (expired included)
};

// The slot's voter: `name` a person on the server, empty when the slot is free or a bot. A voter
// who leaves loses his ballot and an open vote is weighed again.
void vote_seat(VoteBook& b, int slot, std::string_view name, double now);
// A vote whose time ran out at `now` fails.
void vote_tick(VoteBook& b, double now);
// One of the words above for `slot`; false when the line is not the vote's.
bool vote_command(VoteBook& b, const VoteTarget& target, int slot, std::string_view line,
                  double now);
// The changes passed since the last call, in order, for the game to make.
std::vector<VoteChange> vote_take(VoteBook& b);
// The map is over: an open vote is dropped, and every kept change comes back with its `before` as
// the value, newest first, for the game to put back. The book is then clean for the next map.
std::vector<VoteChange> vote_map_over(VoteBook& b);

int vote_voter_count(const VoteBook& b);
int vote_count(const VoteBook& b, Ballot ballot);
// Yes needed to pass: more than half the voters.
int vote_needed(const VoteBook& b);
// Whole seconds left on the open vote, rounded up (0 with none).
int vote_seconds_left(const VoteBook& b, double now);

}  // namespace ge::server
