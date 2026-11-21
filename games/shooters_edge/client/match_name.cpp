// A match's recording name (match_name.hpp).
//
// ge:summary The match name's kind, codes, cleaning, free number and settings, defined
// ge:keywords match_type expand_match_name clean_match_text free_match_file add_match_cvars
// ge:skill shooter replay
// ge:test tests/shooters_edge_match_name_test.cpp
#include "shooters_edge/client/match_name.hpp"

#include <algorithm>
#include <cstdio>
#include <system_error>
#include <utility>

#include "shooters_edge/client/demo_caption.hpp"

namespace shooters_edge::client {

namespace {

namespace con = ge::console;

// The teams in the order first seen among the players, with their sizes.
struct Teams {
  std::vector<std::pair<std::string, int>> list;
  int biggest = 0;  // the biggest named team's size
};

std::vector<const NamedPlayer*> playing(const MatchFacts& f) {
  std::vector<const NamedPlayer*> out;
  for (const NamedPlayer& p : f.players)
    if (!p.spectator && !p.name.empty()) out.push_back(&p);
  return out;
}

Teams teams_of(const std::vector<const NamedPlayer*>& ps) {
  Teams t;
  for (const NamedPlayer* p : ps) {
    auto it = std::find_if(t.list.begin(), t.list.end(),
                           [&](const auto& e) { return e.first == p->team; });
    if (it == t.list.end())
      t.list.emplace_back(p->team, 1);
    else
      it->second++;
  }
  for (const auto& [name, n] : t.list)
    if (!name.empty()) t.biggest = std::max(t.biggest, n);
  return t;
}

const NamedPlayer* me_of(const MatchFacts& f) {
  return f.me >= 0 && static_cast<size_t>(f.me) < f.players.size() ? &f.players[f.me] : nullptr;
}

bool has_word(std::string_view text, std::string_view word) {
  std::string low(text);
  for (char& c : low)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return low.find(word) != std::string::npos;
}

std::string two(int v, int width = 2) {
  char b[16];
  std::snprintf(b, sizeof b, "%0*d", width, v);
  return b;
}

bool unfit(unsigned char c) {
  return c < ' ' || c == ' ' || c == '\\' || c == '/' || c == '?' || c == '*' || c == ':' ||
         c == '<' || c == '>' || c == '"' || c == '|';
}

}  // namespace

std::string match_type(const MatchFacts& f) {
  const std::vector<const NamedPlayer*> ps = playing(f);
  const Teams t = teams_of(ps);
  const size_t n = ps.size();
  if (n == 0) return "";
  if (has_word(f.gamedir, "qw") && f.gamedir.size() == 2 && f.race) return "race";
  if (n == 1) return "solo";
  if (f.fortress) {
    if (n == 2) return "tfduel";
    return t.list.size() < 2 ? "unknown" : "tfclanwar";
  }
  if (has_word(f.gamedir, "arena")) return "arena";
  if (f.deathmatch == 0) return "coop";
  if (n == 2) return f.teamplay == 0 || t.list.size() == 2 ? "duel" : "unknown";
  if (f.teamplay == 0 || t.biggest <= 1) return "ffa";
  if (t.list.size() != 2) return t.list.size() > 2 ? "multiteam" : "unknown";
  if (t.biggest == 2) return "2on2";
  if (t.biggest == 3) return "3on3";
  if (t.biggest == 4) return "4on4";
  return "tdm";
}

std::string match_format_setting(std::string_view type) {
  if (type.empty() || type == "unknown") return "";
  if (type == "tfduel") return "match_format_tf_duel";
  if (type == "tfclanwar") return "match_format_tf_clanwar";
  return "match_format_" + std::string(type);
}

std::string expand_match_name(std::string_view pattern, const MatchFacts& f, const MatchWords& w) {
  const std::vector<const NamedPlayer*> ps = playing(f);
  const Teams t = teams_of(ps);
  const NamedPlayer* me = me_of(f);
  const bool watching = f.spectating || me == nullptr || me->spectator;
  // Player 1 and 2; team 1 and 2.
  const NamedPlayer* p1 = watching ? (ps.empty() ? nullptr : ps[0]) : me;
  const NamedPlayer* p2 = nullptr;
  if (watching) {
    if (ps.size() > 1) p2 = ps[1];
  } else {
    for (const NamedPlayer* p : ps)
      if (p->name != me->name) {
        p2 = p;
        break;
      }
  }
  std::string t1 = watching ? (t.list.empty() ? "" : t.list[0].first) : me->team, t2;
  for (const auto& [name, size] : t.list)
    if (name != t1) {
      t2 = name;
      break;
    }
  const auto clean = [](std::string_view s) { return clean_match_text(s, false); };
  const auto members = [&](const std::string& team) {
    std::string out;
    for (const NamedPlayer* p : ps)
      if (p->team == team) out += (out.empty() ? "" : w.join) + clean(p->name);
    return out;
  };
  const auto size_of = [&](const std::string& team) {
    for (const auto& [name, size] : t.list)
      if (name == team) return size;
    return 0;
  };
  std::string out;
  for (size_t i = 0; i < pattern.size(); i++) {
    if (pattern[i] != '%' || i + 1 >= pattern.size()) {
      out += pattern[i];
      continue;
    }
    const char c = pattern[++i];
    switch (c) {
      case 'n': {
        std::string mine = !w.nick.empty() ? w.nick : (me != nullptr ? me->name : "");
        out += clean(mine);
        if (watching) out += w.spec;
        break;
      }
      case 'p':
        out += p1 != nullptr ? clean(p1->name) : "";
        break;
      case 'e':
        out += ps.size() == 2 ? (p2 != nullptr ? clean(p2->name) : "") : clean(t2);
        break;
      case 't':
        out += clean(t1);
        break;
      case 'k':
        out += members(t1);
        break;
      case 'l':
        out += members(t2);
        break;
      case 'O':
        out += std::to_string(f.teamplay != 0 ? size_of(t1) : 1);
        break;
      case 'E':
        out += std::to_string(f.teamplay != 0 ? size_of(t2)
                                              : std::max(0, static_cast<int>(ps.size()) - 1));
        break;
      case 'C':
        out += std::to_string(ps.size());
        break;
      case 'a':
      case 'b': {
        std::string part;
        for (const auto& [name, size] : t.list) {
          if (!part.empty()) part += c == 'a' ? w.on : w.versus;
          part += c == 'a' ? std::to_string(size) : clean(name);
        }
        out += part;
        break;
      }
      case 'v':
        out += w.versus;
        break;
      case 'T':
        out += std::to_string(f.timelimit);
        break;
      case 'F':
        out += std::to_string(f.fraglimit);
        break;
      case 'N':
        out += std::to_string(f.teamplay);
        break;
      case 'D':
        out += std::to_string(f.deathmatch);
        break;
      case 'M':
        out += f.map;
        break;
      case 'G':
        out += f.gamedir;
        break;
      case 'd':
        out += two(f.day);
        break;
      case 'm':
        out += two(f.month);
        break;
      case 'y':
        out += two(f.year % 100);
        break;
      case 'Y':
        out += two(f.year, 4);
        break;
      case 'H':
        out += two(f.hour);
        break;
      case 'Q':
        out += two(f.minute);
        break;
      case 'S':
        out += two(f.second);
        break;
      case '%':
        out += '%';
        break;
      default:
        out += '%';
        out += c;
        break;
    }
  }
  return out;
}

std::string clean_match_text(std::string_view text, bool path) {
  const std::string plain = path ? std::string(text) : plain_text(text);
  std::string out;
  bool run = false;  // the last character written is a `_` made of an unfit one
  for (const char ch : plain) {
    auto c = static_cast<unsigned char>(ch);
    if (path && c == '\\') c = '/';
    const bool keep = path && (c == ' ' || c == '/');
    if (!keep && unfit(c)) {
      if (!run) out += '_';
      run = true;
      continue;
    }
    run = false;
    out += static_cast<char>(c);
  }
  if (path)
    while (!out.empty() && out[0] == '/') out.erase(0, 1);
  if (out.size() > 1023) out.resize(1023);
  return out;
}

std::string match_name(const MatchFacts& f, const MatchWords& w,
                       const std::function<std::string(const std::string& setting)>& pattern_of) {
  const std::string type = match_type(f);
  std::string pattern;
  if (type.empty())
    pattern = "%n - [%M]";
  else if (type == "unknown")
    pattern = "%n - [Unknown Game, gamedir - %G, %C players] - [%M]";
  else
    pattern = pattern_of(match_format_setting(type));
  return clean_match_text(expand_match_name(pattern, f, w), true);
}

std::string free_match_file(const std::filesystem::path& folder, const std::string& name,
                            std::initializer_list<const char*> exts) {
  for (int n = 0; n < 1000; n++) {
    const std::string stem = name + "_" + two(n, 3);
    bool free = true;
    for (const char* ext : exts) {
      std::error_code ec;
      if (std::filesystem::exists(folder / (stem + "." + ext), ec)) free = false;
    }
    if (free) return stem;
  }
  return "";
}

void add_match_cvars(con::Console& c) {
  const con::CvarLayer layer(c.cvars(), "classic");
  const std::pair<const char*, const char*> formats[] = {
      {"solo", "solo/%n - [%M]"},
      {"coop", "coop/%n - [%C_player_coop] - [%M]"},
      {"race", "race/%n - [race] - [%M]"},
      {"duel", "duel/%n - %p%v%e - [dmm%D] - [%M]"},
      {"ffa", "ffa/%n - [%C_player_ffa] - [%M]"},
      {"2on2", "2on2/%n - [%k%v%l] - [%M]"},
      {"3on3", "tdm/%n - [%Oon%E_%t%v%e] - [%M]"},
      {"4on4", "tdm/%n - [%Oon%E_%t%v%e] - [%M]"},
      {"tdm", "tdm/%n - [%Oon%E_%t%v%e] - [%M]"},
      {"multiteam", "tdm/%n - [%a_%b] - [%M]"},
      {"arena", "arena/%n - %p%v%e - [%F_frags] - [%M]"},
      {"tf_duel", "tfduel/%n - %p%v%e [%M]"},
      {"tf_clanwar", "tfwar/%n - [%Oon%E_%t%v%e] - [%M]"},
  };
  for (const auto& [kind, pattern] : formats) {
    const std::string name = std::string("match_format_") + kind;
    std::string kind_words = kind;
    std::replace(kind_words.begin(), kind_words.end(), '_', ' ');
    const std::string help = "How a recorded " + kind_words + " match's file is named.";
    c.cvars().add({.name = name,
                   .fallback = pattern,
                   .flags = con::kArchive,
                   .help = help,
                   .kind = con::CvarKind::text,
                   .values = "TEXT: the name, with %n you, %p and %e the players, %t the team, "
                             "%M the map, %v the versus word and more"});
  }
  const std::pair<const char*, const char*> words[][2] = {
      {{"match_name_and", "_&_"}, {"Put between the players of a team in a match's name.", ""}},
      {{"match_name_versus", "_vs_"}, {"Put between the sides in a match's name.", ""}},
      {{"match_name_on", "on"}, {"Put between the teams' sizes in a match's name.", ""}},
      {{"match_name_nick", ""}, {"Your name in a match's name, in place of the one you play.", ""}},
      {{"match_name_spec", "(SPEC)"},
       {"Put after your name in a match's name when you only watch.", ""}},
  };
  for (const auto& wd : words)
    c.cvars().add({.name = wd[0].first,
                   .fallback = wd[0].second,
                   .flags = con::kArchive,
                   .help = wd[1].first,
                   .kind = con::CvarKind::text,
                   .values = "TEXT: the words"});
  c.cvars().add({.name = "match_auto_record",
                 .fallback = "0",
                 .flags = con::kArchive,
                 .help = "Whether each match you play on a server is recorded on its own, from "
                         "its start to its end.",
                 .kind = con::CvarKind::integer,
                 .min = 0,
                 .max = 2,
                 .values = "0: never (the default); 1: recorded and kept when you type "
                           "match_save; 2: recorded and saved at the end"});
  c.cvars().add({.name = "match_auto_minlength",
                 .fallback = "30",
                 .flags = con::kArchive,
                 .help = "How long a match cut short must have been recorded to be saved, in "
                         "seconds.",
                 .min = 0,
                 .max = 3600,
                 .values = "30: the default; 0..3600: that many seconds"});
  c.cvars().add({.name = "match_auto_spectating",
                 .fallback = "0",
                 .flags = con::kArchive,
                 .help = "Whether matches you only watch are recorded on their own too.",
                 .kind = con::CvarKind::boolean});
  c.cvars().add({.name = "match_auto_logconsole",
                 .fallback = "1",
                 .flags = con::kArchive,
                 .help = "Whether the console's lines during each match you play on a server "
                         "are kept as a log beside its recording.",
                 .kind = con::CvarKind::integer,
                 .min = 0,
                 .max = 2,
                 .values = "0: never; 1: kept when you type match_save (the default); 2: saved "
                           "at the end"});
}

MatchWords match_words(const con::Cvars& cvars) {
  const auto get = [&](const char* name, const char* fallback) {
    const con::Cvar* v = cvars.find(name);
    return v != nullptr ? v->value : std::string(fallback);
  };
  MatchWords w;
  w.join = get("match_name_and", "_&_");
  w.versus = get("match_name_versus", "_vs_");
  w.on = get("match_name_on", "on");
  w.nick = get("match_name_nick", "");
  w.spec = get("match_name_spec", "(SPEC)");
  return w;
}

}  // namespace shooters_edge::client
