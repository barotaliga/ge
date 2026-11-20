// The chat ignore lists (chat_ignore.hpp).
//
// ge:summary The chat ignore lists' commands, settings and the drop rule
// ge:keywords chat-ignore ignore ignorelist ignore_team chat_ignored chat_new_level
// ge:skill console net
// ge:test tests/console_chat_ignore_test.cpp
#include "console/chat_ignore.hpp"

#include <algorithm>
#include <cstdlib>

#include "console/cvar.hpp"

namespace ge::console {

namespace {

// A name as a player reads it: a charset's high-bit letters plain, without case.
std::string plain(const std::string& s) {
  std::string out;
  for (const char ch : s) {
    char c = static_cast<char>(static_cast<unsigned char>(ch) & 0x7f);
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    out += c;
  }
  return out;
}

const std::string& at(const std::array<std::string, kChatPlaces>& a, int k) {
  return a[static_cast<size_t>(k)];
}

bool all_digits(const std::string& a) {
  return !a.empty() && std::all_of(a.begin(), a.end(), [](char c) { return c >= '0' && c <= '9'; });
}

ChatRoster roster_of(const ChatIgnore& g) { return g.roster ? g.roster() : ChatRoster{}; }

bool watching(const ChatRoster& r) {
  return r.me >= 0 && r.me < kChatPlaces && r.spectator[static_cast<size_t>(r.me)];
}

bool my_team(const ChatRoster& r, int k) {
  return r.me >= 0 && r.me < kChatPlaces && at(r.teams, k) == at(r.teams, r.me);
}

// The place `a` names; -1 none.
int find_player(const ChatRoster& r, const std::string& a, bool number_only) {
  const auto used = [&](int k) { return !at(r.names, k).empty(); };
  if (all_digits(a)) {
    const int k = std::atoi(a.c_str()) - 1;
    if (k >= 0 && k < kChatPlaces && used(k)) return k;
  }
  if (number_only) return -1;
  for (int k = 0; k < kChatPlaces; ++k)
    if (used(k) && at(r.names, k) == a) return k;
  const std::string p = plain(a);
  for (int k = 0; k < kChatPlaces; ++k)
    if (used(k) && plain(at(r.names, k)) == p) return k;
  for (int k = 0; k < kChatPlaces; ++k)
    if (used(k) && !p.empty() && plain(at(r.names, k)).find(p) != std::string::npos) return k;
  return -1;
}

void forget_new_players(ChatIgnore& g, const ChatRoster& r) {
  for (int k = 0; k < kChatPlaces; ++k) {
    std::string& seen = g.seen[static_cast<size_t>(k)];
    const std::string& now = at(r.names, k);
    if (seen.empty() && !now.empty()) g.ignored[static_cast<size_t>(k)] = false;
    seen = now;
  }
}

void list(Console& c, ChatIgnore& g) {
  const ChatRoster r = roster_of(g);
  forget_new_players(g, r);
  std::string players;
  for (int k = 0; k < kChatPlaces; ++k)
    if (g.ignored[static_cast<size_t>(k)] && !at(r.names, k).empty())
      players += (players.empty() ? "" : ", ") + at(r.names, k);
  c.print("Ignored players: " + (players.empty() ? std::string("none") : players));
  std::string teams;
  for (const std::string& t : g.teams) teams += (teams.empty() ? "" : ", ") + t;
  c.print("Ignored teams: " + (teams.empty() ? std::string("none") : teams));
  if (g.opponents != 0) c.print("Opponents are ignored.");
  if (g.spec == 2 || (g.spec == 1 && !watching(r))) c.print("Spectators are ignored.");
  if (g.tv) c.print("TV viewers are ignored.");
}

void player_command(Console& c, ChatIgnore& g, const Args& a, bool on, bool number_only) {
  if (a.size() < 2) {
    if (on)
      list(c, g);
    else
      c.print("Name the player to stop ignoring.");
    return;
  }
  const ChatRoster r = roster_of(g);
  forget_new_players(g, r);
  const int k = find_player(r, a[1], number_only);
  if (k < 0) {
    c.print("No player called " + a[1] + " is here.");
    return;
  }
  const std::string& name = at(r.names, k);
  bool& flag = g.ignored[static_cast<size_t>(k)];
  if (flag == on) {
    c.print(on ? name + " is already ignored." : name + " is not being ignored.");
    return;
  }
  flag = on;
  c.print(on ? "Ignoring " + name + "." : "No longer ignoring " + name + ".");
}

void team_command(Console& c, ChatIgnore& g, const Args& a, bool on) {
  if (a.size() < 2) {
    list(c, g);
    return;
  }
  const std::string& team = a[1];
  const auto found = std::find(g.teams.begin(), g.teams.end(), team);
  if (!on) {
    if (found == g.teams.end()) {
      c.print("Team " + team + " is not being ignored.");
      return;
    }
    g.teams.erase(found);
    c.print("No longer ignoring team " + team + ".");
    return;
  }
  if (found != g.teams.end()) {
    c.print("Team " + team + " is already ignored.");
    return;
  }
  const ChatRoster r = roster_of(g);
  bool playing = false;
  for (int k = 0; k < kChatPlaces; ++k)
    playing = playing || (!at(r.names, k).empty() && !r.spectator[static_cast<size_t>(k)] &&
                          at(r.teams, k) == team);
  if (!playing) {
    c.print("No team called " + team + " is playing.");
    return;
  }
  if (static_cast<int>(g.teams.size()) >= kChatIgnoreTeams) {
    c.print("You can ignore at most " + std::to_string(kChatIgnoreTeams) + " teams.");
    return;
  }
  g.teams.push_back(team);
  c.print("Ignoring team " + team + ".");
}

// The line as a filter reads it: each character's top bit cleared, the unprintable left out.
std::string cleaned(std::string_view text) {
  std::string out;
  for (const char ch : text) {
    const char c = static_cast<char>(static_cast<unsigned char>(ch) & 0x7f);
    if (c >= ' ' && c < 0x7f) out += c;
  }
  return out;
}

bool nick_said(const std::string& nick, const std::string& line) {
  const auto said_at = [&](size_t at) {
    const std::string_view rest = std::string_view(line).substr(at);
    return rest.starts_with(nick + ": ") || rest.starts_with("(" + nick + "): ");
  };
  if (said_at(0)) return true;
  if (line.empty() || line[0] != '[') return false;
  const size_t close = line.find("] ");
  return close != std::string::npos && said_at(close + 2);
}

// `<cmd> <words>` adds a filter, `un<cmd> <words>|all` takes it away; alone, the list.
void filter_command(Console& c, std::vector<ChatIgnore::Filter>& list, const Args& a, bool on,
                    bool nick) {
  const auto print_list = [&] {
    if (list.empty()) {
      c.print("No ignore filters.");
      return;
    }
    c.print("Current ignore filters:");
    for (const ChatIgnore::Filter& f : list)
      c.print("\"" + f.match + "\" (" + std::to_string(f.hits) +
              (f.hits == 1 ? " hit)" : " hits)"));
  };
  if (a.size() < 2) {
    print_list();
    return;
  }
  // A nick is its first word (a second word lets `unignorenick all` name a player called all).
  std::string match = a[1];
  if (!nick)
    for (size_t i = 2; i < a.size(); ++i) match += " " + a[i];
  const auto found = std::find_if(list.begin(), list.end(),
                                  [&](const ChatIgnore::Filter& f) { return f.match == match; });
  if (on) {
    if (match.size() < (nick ? 1u : 3u)) {
      c.print("Match string \"" + match + "\" is too short.");
      return;
    }
    if (found == list.end()) list.push_back({match, 0});
    return;
  }
  if (match == "all" && a.size() == 2) {
    c.print("Removed " + std::to_string(list.size()) + " ignore filter(s).");
    list.clear();
    return;
  }
  if (found == list.end()) {
    c.print("Can't find ignore filter \"" + match + "\"");
    return;
  }
  list.erase(found);
}

void add_settings(Cvars& v, ChatIgnore& g) {
  v.bind({.name = "ignore_mode",
          .fallback = "0",
          .flags = kArchive,
          .help = "Whether an ignored player's team chat is dropped too.",
          .kind = CvarKind::boolean,
          .values = "0: their team chat still shows (the default); 1: it goes too"},
         &g.team_too);
  v.bind({.name = "ignore_opponents",
          .fallback = "0",
          .flags = kArchive,
          .help = "Whether your opponents' chat to everyone is dropped.",
          .kind = CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: never (the default); 1: always; 2: while a match runs"},
         &g.opponents);
  v.bind({.name = "ignore_spec",
          .fallback = "0",
          .flags = kArchive,
          .help = "Whether spectators' chat is dropped.",
          .kind = CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: never (the default); 1: while you play; 2: always"},
         &g.spec);
  v.bind({.name = "ignore_flood",
          .fallback = "0",
          .flags = kArchive,
          .help = "Whether a chat line said again soon after is dropped.",
          .kind = CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: never (the default); 1: chat to everyone; 2: team chat too"},
         &g.flood);
  v.bind({.name = "ignore_flood_duration",
          .fallback = "4",
          .flags = kArchive,
          .help = "How long a chat line counts as said again, in seconds.",
          .min = 0,
          .max = 10,
          .values = "0..10: that many seconds (4 the default)"},
         &g.flood_s);
  v.bind({.name = "ignore_tv",
          .fallback = "0",
          .flags = kArchive,
          .help = "Whether TV viewers' chat is dropped.",
          .kind = CvarKind::boolean,
          .values = "0: shown (the default); 1: dropped"},
         &g.tv);
}

std::string lower_ascii(std::string_view s) {
  std::string out(s);
  for (char& ch : out)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return out;
}

// `filter`: the team tags replaced, cleared or listed.
void tags_command(Console& k, ChatIgnore& g, const Args& a) {
  if (a.size() < 2) {
    if (g.tags.empty()) {
      k.print("No team tags are set.");
      return;
    }
    std::string line;
    for (const std::string& t : g.tags) line += (line.empty() ? "#" : " #") + t;
    k.print(line);
    return;
  }
  g.tags.clear();
  if (a.size() == 2 && (a[1].empty() || lower_ascii(a[1]) == "clear")) {
    k.print("Team tags cleared.");
    return;
  }
  for (size_t i = 1; i < a.size() && g.tags.size() < static_cast<size_t>(kChatTags); ++i) {
    const std::string& w = a[i];
    if (w.empty() || w[0] != '#') {
      k.print("A team tag starts with #, as in #def.");
      return;
    }
    if (w.find(' ') != std::string::npos) {
      k.print("A team tag has no spaces.");
      return;
    }
    g.tags.push_back(lower_ascii(std::string_view(w).substr(1, 4)));
  }
}

}  // namespace

bool chat_tag_hidden(const ChatIgnore& g, std::string& text) {
  if (text.find("#inlay#") != std::string::npos) return true;
  if (g.tags.empty()) return false;
  size_t end = text.size();
  while (end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r')) --end;
  // The last word: `#` and 1..4 letters, a space or the line's start before it.
  const size_t hash = text.rfind('#', end == 0 ? 0 : end - 1);
  if (hash == std::string::npos || hash == 0 || end - hash < 2 || end - hash > 5) return false;
  if (text.find(' ', hash) < end) return false;
  if (hash >= 2 && text[hash - 1] == ' ' && text[hash - 2] == ':') return false;  // all the text
  const std::string tag = lower_ascii(std::string_view(text).substr(hash + 1, end - hash - 1));
  if (std::find(g.tags.begin(), g.tags.end(), tag) == g.tags.end()) return true;
  const size_t from = text[hash - 1] == ' ' ? hash - 1 : hash;
  text.erase(from, end - from);
  return false;
}

void add_chat_ignore(Console& c, ChatIgnore& g) {
  add_settings(c.cvars(), g);
  c.add_command("filter",
                "filter [#tag ...|clear]: show only the team lines ending with these tags",
                [&g](Console& k, const Args& a) { tags_command(k, g, a); });
  c.add_command("ignore", "ignore <name or number>: drop a player's chat",
                [&g](Console& k, const Args& a) { player_command(k, g, a, true, false); });
  c.add_command("ignore_id", "ignore_id <number>: drop the chat of the player of that number",
                [&g](Console& k, const Args& a) { player_command(k, g, a, true, true); });
  c.add_command("unignore", "unignore <name or number>: show a player's chat again",
                [&g](Console& k, const Args& a) { player_command(k, g, a, false, false); });
  c.add_command("unignore_id", "unignore_id <number>: show that player's chat again",
                [&g](Console& k, const Args& a) { player_command(k, g, a, false, true); });
  c.add_command("ignorelist", "ignorelist: the players and teams ignored",
                [&g](Console& k, const Args&) { list(k, g); });
  c.add_command("ignore_team", "ignore_team <team>: drop a team's chat",
                [&g](Console& k, const Args& a) { team_command(k, g, a, true); });
  c.add_command("unignore_team", "unignore_team <team>: show a team's chat again",
                [&g](Console& k, const Args& a) { team_command(k, g, a, false); });
  c.add_command("ignoretext", "ignoretext <words>: drop a chat line that is those words (* and ?)",
                [&g](Console& k, const Args& a) { filter_command(k, g.texts, a, true, false); });
  c.add_command("unignoretext", "unignoretext <words>|all: show those chat lines again",
                [&g](Console& k, const Args& a) { filter_command(k, g.texts, a, false, false); });
  c.add_command("ignorenick", "ignorenick <name>: drop the chat said by that name",
                [&g](Console& k, const Args& a) { filter_command(k, g.nicks, a, true, true); });
  c.add_command("unignorenick", "unignorenick <name>|all: show that name's chat again",
                [&g](Console& k, const Args& a) { filter_command(k, g.nicks, a, false, true); });
  c.add_command("unignoreall", "unignoreall: show every player's chat again",
                [&g](Console& k, const Args&) {
                  g.ignored.fill(false);
                  k.print("No players are ignored now.");
                });
  c.add_command("unignoreall_team", "unignoreall_team: show every team's chat again",
                [&g](Console& k, const Args&) {
                  g.teams.clear();
                  k.print("No teams are ignored now.");
                });
}

bool chat_text_matches(std::string_view match, std::string_view text) {
  // Greedy with one step back: the last `*` seen takes one more character on a miss.
  size_t m = 0, t = 0, star = std::string_view::npos, after = 0;
  while (t < text.size()) {
    if (m < match.size() && match[m] == '*') {
      star = m++;
      after = t;
    } else if (m < match.size() && match[m] == '\\' && m + 1 < match.size() &&
               match[m + 1] == text[t]) {
      m += 2;
      ++t;
    } else if (m < match.size() && match[m] != '\\' && (match[m] == '?' || match[m] == text[t])) {
      ++m;
      ++t;
    } else if (star != std::string_view::npos) {
      m = star + 1;
      t = ++after;
    } else {
      return false;
    }
  }
  while (m < match.size() && match[m] == '*') ++m;
  return m == match.size();
}

bool chat_ignored(ChatIgnore& g, const ChatRoster& r, const ChatLine& line, double now_s) {
  forget_new_players(g, r);
  if ((line.chat || line.kind != ChatKind::none) && !(g.texts.empty() && g.nicks.empty())) {
    const std::string seen = cleaned(line.text);
    for (ChatIgnore::Filter& f : g.texts) {
      if (!chat_text_matches(f.match, seen)) continue;
      ++f.hits;
      return true;
    }
    for (ChatIgnore::Filter& f : g.nicks) {
      if (!nick_said(f.match, seen)) continue;
      ++f.hits;
      return true;
    }
  }
  if (line.kind == ChatKind::tv) return g.tv;
  if (line.kind == ChatKind::none || line.place < 0 || line.place >= kChatPlaces) return false;
  const int k = line.place;
  const bool me = k == r.me;
  const bool team_line = line.kind == ChatKind::team;
  // The player and team lists (team chat only with ignore_mode 1).
  if (!team_line || g.team_too) {
    if (g.ignored[static_cast<size_t>(k)]) return true;
    if (r.teams_play && !r.spectator[static_cast<size_t>(k)] && !me &&
        std::find(g.teams.begin(), g.teams.end(), at(r.teams, k)) != g.teams.end())
      return true;
  }
  if (line.kind == ChatKind::spectator && (g.spec == 2 || (g.spec == 1 && !watching(r))))
    return true;
  if (line.kind == ChatKind::spectator_team && g.spec == 2) return true;
  if (line.kind == ChatKind::everyone && g.opponents != 0 && !watching(r) && !me &&
      (!r.teams_play || !my_team(r, k)) && (g.opponents == 1 || r.match_running))
    return true;
  // The flood: the same line again soon after one kept.
  const bool to_all = line.kind == ChatKind::everyone || line.kind == ChatKind::spectator;
  if (g.flood == 0 || me || !(to_all || g.flood == 2)) return false;
  for (int i = 0; i < kChatFloodKept; ++i)
    if (g.kept[static_cast<size_t>(i)] == line.text &&
        now_s - g.kept_at[static_cast<size_t>(i)] < g.flood_s)
      return true;
  g.kept[static_cast<size_t>(g.next)] = line.text;
  g.kept_at[static_cast<size_t>(g.next)] = now_s;
  g.next = (g.next + 1) % kChatFloodKept;
  return false;
}

void chat_new_level(ChatIgnore& g) {
  g.ignored.fill(false);
  g.seen.fill({});
  g.kept.fill({});
}

}  // namespace ge::console
