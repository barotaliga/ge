// CHAT IGNORE LISTS: any multiplayer game's way to stop reading a player's chat. The game reads
// each incoming chat line into a `ChatLine` (who said it and how) and keeps a `ChatRoster` of the
// players; the engine's lists and settings decide which lines go.
//
// THE COMMANDS (`add_chat_ignore`):
//   ignore <name or number>    a player's chat dropped: a number of digits is the player's number
//                              (1 the first place), else the name exactly, then without case, then
//                              any player whose name holds the words; a flag on the place, kept
//                              through a rename, gone when another player takes the place
//   ignore_id <number>         the same by number alone
//   unignore, unignore_id      shown again
//   ignorelist                 the players and teams ignored and the settings that drop lines
//   ignore_team <team>         a team playing now (at most 8), judged by the speaker's team at each
//                              line in a team game, never a spectator's line or your own; kept
//                              until quit
//   unignore_team, unignoreall, unignoreall_team
// THE SETTINGS:
//   ignore_mode 0|1            1: an ignored player's (or team's) team chat goes too
//   ignore_opponents 0..2      chat to everyone from players not on your team (every other player
//                              outside a team game) while you play: 1 always, 2 while a match runs
//   ignore_spec 0..2           spectators' chat to everyone: 1 while you play, 2 always (and their
//                              own chat among themselves)
//   ignore_flood 0..2          a line the same as one kept less than ignore_flood_duration (4) s
//                              ago among the last 10: 1 chat to everyone, 2 team chat too; never
//                              your own
//   ignore_tv 0|1              chat from TV viewers
// THE FILTERS (kept until quit, any server):
//   ignoretext <words>         a chat line that is wholly these words is dropped; `*` stands for
//   any
//                              run of characters, `?` for one, `\` takes the next as written; case
//                              counts; at least 3 characters; alone, the filters with their hits
//   unignoretext <words>|all   the filter of exactly those words, or every one, taken away
//   ignorenick <name>          a chat line said by that name (`name: `, `(name): `, after a leading
//                              `[...] `), case counting, whoever has it now
//   unignorenick <name>|all
//   The line is matched with each character's top bit cleared and every unprintable one left out.
//   filter [#tag ...|clear]    the team's tags (at most 8, each cut to 4 letters, any case): a team
//                              line whose last word is `#` and 1..4 letters shows only when its tag
//                              is one of them, the tag left off; an untagged line always shows;
//                              alone, the tags. A team line holding `#inlay#` never shows.
// `chat_new_level` clears the players' flags and the flood's memory (a new map, a new server).
//
// ge:summary Chat ignore lists any multiplayer game reads its chat lines through
// ge:keywords chat-ignore ignore unignore ignorelist ignore_team ignore_mode ignore_opponents
// ge:keywords ignore_spec ignore_flood ignore_flood_duration ignore_tv mute-chat chat_ignored
// ge:keywords ignoretext unignoretext ignorenick unignorenick chat-filter wildcard filter
// ge:keywords team-tags chat_tag_hidden
// ge:skill console net
// ge:test tests/console_chat_ignore_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace ge::console {

inline constexpr int kChatPlaces = 32;
inline constexpr int kChatIgnoreTeams = 8;
inline constexpr int kChatFloodKept = 10;
inline constexpr int kChatTags = 8;

// The players as the game knows them now: by place, "" an empty place.
struct ChatRoster {
  std::array<std::string, kChatPlaces> names;
  std::array<std::string, kChatPlaces> teams;
  std::array<bool, kChatPlaces> spectator{};
  int me = -1;                // the local player's place
  bool teams_play = false;    // a team game
  bool match_running = true;  // a match is on (not a warmup or a standby)
};

enum class ChatKind : uint8_t {
  none,            // not a player's chat
  everyone,        // a player's chat to everyone
  team,            // a player's team chat (to the local player's team, or the local player's own)
  spectator,       // a spectator's chat to everyone
  spectator_team,  // spectators' chat among themselves
  tv,              // a TV viewer's chat
};

struct ChatLine {
  ChatKind kind = ChatKind::none;
  int place = -1;     // the speaker (none for tv)
  std::string text;   // the whole line, for the flood
  bool chat = false;  // a chat line at all (the filters read it, a speaker known or not)
};

struct ChatIgnore {
  // The settings.
  bool team_too = false;  // ignore_mode
  int opponents = 0, spec = 0, flood = 0;
  double flood_s = 4;
  bool tv = false;
  // The lists.
  std::array<bool, kChatPlaces> ignored{};
  std::array<std::string, kChatPlaces> seen;  // each place's name when last looked
  std::vector<std::string> teams;
  // The filters, each with how many lines it dropped.
  struct Filter {
    std::string match;
    int hits = 0;
  };
  std::vector<Filter> texts, nicks;
  std::vector<std::string> tags;  // `filter`'s, lower case, without the `#`
  // The flood's memory: the last lines kept, with when.
  std::array<std::string, kChatFloodKept> kept;
  std::array<double, kChatFloodKept> kept_at{};
  int next = 0;
  // The players now, for the commands (the game's; null: nobody).
  std::function<ChatRoster()> roster;
};

// The commands and settings over `g` (`g` must outlive `con`).
void add_chat_ignore(Console& con, ChatIgnore& g);
// Whether `text` matches the filter pattern `match` (`*`, `?`, `\`), wholly, case counting.
bool chat_text_matches(std::string_view match, std::string_view text);
// Whether `line` is dropped at `now_s` (any clock in seconds); a kept line is remembered for the
// flood. A place newly taken since the last call drops its flag first.
bool chat_ignored(ChatIgnore& g, const ChatRoster& r, const ChatLine& line, double now_s);
// The team tags' rule on a team line `text` (its newline, if any, at the end): true when it is
// hidden; else the tag it ends with, if one of `g.tags`, is taken off `text`.
bool chat_tag_hidden(const ChatIgnore& g, std::string& text);
// A new map or server: the players' flags and the flood's memory cleared (the teams stay).
void chat_new_level(ChatIgnore& g);

}  // namespace ge::console
