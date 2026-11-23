// A chat line's sound, filter, stamp and mark (chat_lines.hpp).
//
// ge:summary The settings of a chat line's extras and the extras on each kept line
// ge:keywords chat-sound msg_filter con_timestamps con_highlight chat_line_extras
// ge:skill net shooter
// ge:test tests/shooters_edge_chat_ignore_test.cpp
#include "shooters_edge/client/chat_lines.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "classic/rulesets.hpp"
#include "console/cvar.hpp"
#include "shooters_edge/client/cvar_layers.hpp"

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

constexpr int kChatLevel = 3;
constexpr const char* kTalk = "misc/talk.wav";
constexpr const char* kTalk1 = "misc/talk1.wav";
constexpr size_t kSeenKept = 16;  // the addresses followip remembers
constexpr int kMostHudLines = 32;

std::string info(const DemoState& s, const char* key) {
  for (const auto& [k, v] : s.serverinfo)
    if (k == key) return v;
  return {};
}

bool lower_is(std::string a, const char* b) {
  for (char& c : a)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return a == b;
}

int kind_of(const con::ChatLine& line) {
  switch (line.kind) {
    case con::ChatKind::everyone:
      return kChatFirst;
    case con::ChatKind::team:
      return kChatTeam;
    case con::ChatKind::spectator:
    case con::ChatKind::spectator_team:
      return kChatSpec;
    default:
      return kChatOther;
  }
}

std::string two(int n, char pad) {
  return std::string(1, n < 10 ? pad : static_cast<char>('0' + n / 10)) +
         static_cast<char>('0' + n % 10);
}

}  // namespace

void add_chat_extras(con::Console& c, ChatExtras& x) {
  con::CvarLayer classic(c.cvars(), kLayerClassic);
  con::Cvars& v = c.cvars();
  v.bind({.name = "s_chat_custom",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Which chat lines play a sound.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: none; 1: every chat line (the default); 2: team lines only"},
         &x.sound);
  const char* kinds[kChatKinds] = {"mm1", "mm2", "spec", "otherchat"};
  const char* whose[kChatKinds] = {"a line to everyone", "a team line", "a spectator's line",
                                   "any other chat line"};
  const char* olds[kChatKinds] = {"mm1", "mm2", "spec", "other"};
  for (int k = 0; k < kChatKinds; ++k) {
    const std::string file = std::string("s_") + kinds[k] + "_file";
    const std::string volume = std::string("s_") + kinds[k] + "_volume";
    const std::string file_help = std::string("The sound ") + whose[k] +
                                  " plays, under the game's sound folder (yours only while you "
                                  "watch: a player hears the game's own).";
    const std::string volume_help = std::string("How loud ") + whose[k] + "'s sound is.";
    v.bind({.name = file,
            .fallback = kTalk,
            .flags = con::kArchive,
            .help = file_help,
            .kind = con::CvarKind::text,
            .values = "a file: a sound under the game's sound folder"},
           &x.file[static_cast<size_t>(k)]);
    v.bind({.name = volume,
            .fallback = "1",
            .flags = con::kArchive,
            .help = volume_help,
            .min = 0,
            .max = 1,
            .values = "1: full (the default); 0..1: that share; 0: silent"},
           &x.volume[static_cast<size_t>(k)]);
    v.add_synonym(std::string("con_sound_") + olds[k] + "_file", file);
    v.add_synonym(std::string("con_sound_") + olds[k] + "_volume", volume);
  }
  v.add_synonym("cl_chatsound", "s_chat_custom");
  v.bind({.name = "msg_filter",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Which chat lines are hidden.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 3,
          .values = "0: none (the default); 1: lines to everyone and spectators' lines; 2: team "
                    "lines; 3: both"},
         &x.filter);
  v.bind({.name = "con_timestamps",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether each chat line begins with the time it came.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: no time (the default); 1: hours and minutes; 2: seconds too while you "
                    "watch or before the match"},
         &x.timestamps);
  v.bind({.name = "con_highlight",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether a line that names you is marked.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 3,
          .values = "0: not marked (the default); 1: drawn plain; 2: begun with "
                    "con_highlight_mark; 3: both"},
         &x.highlight);
  v.bind({.name = "con_highlight_mark",
          .fallback = "",
          .flags = con::kArchive,
          .help = "The words a line that names you begins with (con_highlight 2 or 3).",
          .kind = con::CvarKind::text,
          .values = "TEXT: the words; empty: none (the default)"},
         &x.mark);
  v.bind({.name = "cl_chat_notify",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether chat lines also show in the messages at the top of the screen.",
          .kind = con::CvarKind::boolean,
          .values = "1: they do (the default); 0: only the console and the chat lines on screen "
                    "have them"},
         &x.notify);
  v.bind({.name = "cl_chat_filter",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether a chat line is cut at its first line break and cleaned of letters "
                  "that do not print.",
          .kind = con::CvarKind::boolean,
          .values = "0: shown as sent (the default); 1: cut and cleaned"},
         &x.strip);
  v.bind({.name = "scr_chathud",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether the last chat lines stay on screen at the lower left.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: off (the default); 1: in the plain colour; 2: in the alternate colour"},
         &x.hud);
  v.bind({.name = "scr_chathud_lines",
          .fallback = "4",
          .flags = con::kArchive,
          .help = "How many chat lines stay on screen.",
          .kind = con::CvarKind::integer,
          .min = 1,
          .max = kMostHudLines,
          .values = "4: four (the default); 1..32: that many"},
         &x.hud_lines);
  v.bind({.name = "scr_chathud_time",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "How long a chat line stays on screen, in seconds, fading over its last one.",
          .min = 0,
          .max = 3600,
          .values = "0: until newer lines push it off (the default); SECONDS: that long"},
         &x.hud_time);
  v.bind({.name = "cl_nofake",
          .fallback = "2",
          .flags = con::kArchive,
          .help = "Whether a chat line may hide who said it behind a line break.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: lines as sent; 1: line breaks shown as blanks in every chat line; 2: in "
                    "every one but your own team's (the default)"},
         &x.nofake);
  v.bind({.name = "tp_soundtrigger",
          .fallback = "~",
          .flags = con::kArchive,
          .help = "The characters that start a sound's name at the end of a teammate's line.",
          .kind = con::CvarKind::text,
          .values = "\"\": off; CHARACTERS: any of them starts the name (~ the default)"},
         &x.sound_trigger);
  v.bind({.name = "cl_queries",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether you answer f_version, !version and f_server asked in chat.",
          .kind = con::CvarKind::boolean},
         &x.queries);
  c.add_macro("lastip", [&x] { return x.seen.empty() ? std::string() : x.seen.back(); });
  c.add_command("clearchathud", "clearchathud: the chat lines on screen taken away",
                [&x](con::Console&, const con::Args&) { x.feed.clear(); });
  c.add_command("followip",
                "followip [n]: join the server whose address a chat line named last (or the "
                "n-th last)",
                [&x](con::Console& cc, const con::Args& a) {
                  int n = 1;
                  if (a.size() > 1) n = std::atoi(a[1].c_str());
                  if (n < 1 || static_cast<size_t>(n) > x.seen.size()) {
                    cc.print(x.seen.empty() ? "No address has been seen in chat yet."
                                            : "Only " + std::to_string(x.seen.size()) +
                                                  " addresses have been seen in chat.");
                    return;
                  }
                  const std::string& to = x.seen[x.seen.size() - static_cast<size_t>(n)];
                  cc.print("Following " + to + "...");
                  cc.append("connect " + to);
                });
  c.add_command("trigger",
                "trigger [<command> <match>]: run a command when a server message matches; "
                "alone: the list",
                [&x](con::Console& cc, const con::Args& a) {
                  if (a.size() < 3) {
                    if (x.triggers.empty()) cc.print("No message triggers are set.");
                    for (const ChatExtras::Trigger& t : x.triggers)
                      cc.print("\"" + t.command + "\" for \"" + t.match + "\"");
                    return;
                  }
                  for (const ChatExtras::Trigger& t : x.triggers)
                    if (t.command == a[1] && t.match == a[2]) return;  // set already
                  x.triggers.push_back({a[1], a[2]});
                });
  c.add_command("untrigger",
                "untrigger <command> <match>: a message trigger taken away; untrigger all: "
                "every one",
                [&x](con::Console& cc, const con::Args& a) {
                  if (a[1] == "all") {
                    x.triggers.clear();
                    return;
                  }
                  if (a.size() < 3) {
                    cc.print("Give the command and the match, or all.");
                    return;
                  }
                  const auto n = std::erase_if(x.triggers, [&](const ChatExtras::Trigger& t) {
                    return t.command == a[1] && t.match == a[2];
                  });
                  if (n == 0) cc.print("No such message trigger.");
                });
}

void add_q2_chat_sound(con::Console& c, ChatExtras& x) {
  c.cvars().bind({.name = "cl_chat_sound",
                  .fallback = "1",
                  .flags = con::kArchive,
                  .help = "The sound a chat line plays.",
                  .kind = con::CvarKind::text,
                  .values = "0: none; 1: misc/talk.wav (the default); 2: misc/talk1.wav; a sound's "
                            "name: that one"},
                 &x.named_sound);
}

const char* q2_chat_sound(const std::string& named) {
  if (named.empty() || named == "0") return nullptr;
  if (named == "1") return kTalk;
  if (named == "2") return kTalk1;
  return named.find("..") == std::string::npos ? named.c_str() : kTalk;
}

std::vector<std::string> addresses_in(std::string_view text) {
  std::vector<std::string> out;
  const auto digit = [&](size_t i) { return i < text.size() && text[i] >= '0' && text[i] <= '9'; };
  for (size_t i = 0; i < text.size(); ++i) {
    if (!digit(i) || (i > 0 && (digit(i - 1) || text[i - 1] == '.'))) continue;
    size_t at = i;
    int parts = 0;
    bool ok = true;
    for (; parts < 4; ++parts) {
      size_t n = 0;
      int value = 0;
      while (digit(at) && n < 4) value = value * 10 + (text[at++] - '0'), ++n;
      if (n == 0 || n > 3 || value > 255) {
        ok = false;
        break;
      }
      if (parts < 3) {
        if (at >= text.size() || text[at] != '.') {
          ok = false;
          break;
        }
        ++at;
      }
    }
    if (!ok || digit(at) || (at + 1 < text.size() && text[at] == '.' && digit(at + 1))) continue;
    if (at + 1 < text.size() && text[at] == ':' && digit(at + 1)) {
      size_t n = 0;
      ++at;
      while (digit(at) && n < 6) ++at, ++n;
    }
    out.emplace_back(text.substr(i, at - i));
    i = at;
  }
  return out;
}

void run_triggers(ChatExtras& x, std::string_view text) {
  if (x.relay || x.con == nullptr || x.triggers.empty()) return;
  std::string plain;
  for (char ch : text) {
    const auto c = static_cast<char>(static_cast<unsigned char>(ch) & 0x7f);
    if (c != '\n' && c != '\r') plain += c;
  }
  for (const ChatExtras::Trigger& t : x.triggers)
    if (con::chat_text_matches(t.match, plain)) x.con->append(t.command);
}

void tell_feeds(ChatExtras& x, ge::hud::Feed& notify, std::string_view line, bool chat,
                ge::ui::Color ink, double at, const ge::console::ConsoleView::NotifySettings* ns) {
  if (chat && x.hud != 0) {
    x.feed.configure(static_cast<size_t>(std::clamp(x.hud_lines, 1, kMostHudLines)),
                     x.hud_time > 0 ? std::max(0.0, x.hud_time - 1) : -1);
    x.feed.push(line, x.hud == 2 ? ge::ui::Color{0.45f, 1, 0.45f, 1} : ink, at);
  }
  if (ns != nullptr && (!ns->on || ns->lines <= 0)) return;
  if (ns != nullptr) notify.configure(static_cast<size_t>(ns->lines), ns->time_s);
  if (!chat || x.notify != 0) notify.push(line, ink, at);
}

ChatClock chat_clock_now() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  return {local.tm_hour, local.tm_min, local.tm_sec};
}

bool chat_line_extras(ChatExtras& x, DemoState& s, const con::ChatLine& line, DemoPrint& p,
                      const ChatClock& now) {
  const bool watching = s.player >= 0 && static_cast<size_t>(s.player) < s.spectator.size() &&
                        s.spectator[static_cast<size_t>(s.player)];
  const std::string me = s.player >= 0 && static_cast<size_t>(s.player) < s.names.size()
                             ? s.names[static_cast<size_t>(s.player)]
                             : std::string();
  if (!me.empty() && x.highlight >= 2 && p.text.find(me, 2) != std::string::npos)
    p.text = x.mark + p.text;
  if (p.level != kChatLevel) return true;
  if (x.strip != 0) {  // cl_chat_filter: the first line alone, its unprintable letters gone
    std::string kept;
    for (char ch : p.text) {
      if (ch == '\n') break;
      const auto u = static_cast<unsigned char>(ch);
      if ((u & 0x7f) >= 32 && (u & 0x7f) != 127) kept += ch;
    }
    p.text = kept + "\n";
  }
  for (std::string& a : addresses_in(p.text)) {  // followip's
    std::erase(x.seen, a);
    x.seen.push_back(std::move(a));
    if (x.seen.size() > kSeenKept) x.seen.erase(x.seen.begin());
  }
  const int kind = kind_of(line);
  if (((x.filter & 1) != 0 && (kind == kChatFirst || kind == kChatSpec)) ||
      ((x.filter & 2) != 0 && kind == kChatTeam))
    return false;
  if (x.timestamps != 0) {
    const bool seconds = x.timestamps == 2 && (watching || lower_is(info(s, "status"), "standby"));
    p.text = two(now.hour, ' ') + ":" + two(now.minute, '0') +
             (seconds ? ":" + two(now.second, '0') : std::string()) + " " + p.text;
  }
  if (!x.named_sound.empty()) {  // Quake II's cl_chat_sound rules while it is loaded
    if (const char* name = q2_chat_sound(x.named_sound)) {
      DemoSound d;
      d.entity = s.player + 1;
      d.name = name;
      d.volume = 1;
      d.attenuation = 0;
      s.started.push_back(d);
    }
    return true;
  }
  if (x.sound == 0 || (x.sound == 2 && kind != kChatTeam)) return true;
  const float volume = x.volume[static_cast<size_t>(kind)];
  if (!(volume > 0)) return true;
  const std::string& file = x.file[static_cast<size_t>(kind)];
  const bool own =
      (watching || kind == kChatOther) && !file.empty() && file.find("..") == std::string::npos;
  DemoSound d;
  d.entity = s.player + 1;
  d.name = own ? file.c_str() : kTalk;
  d.volume = volume > 1 ? 1 : volume;
  d.attenuation = 0;  // heard everywhere
  s.started.push_back(d);
  return true;
}

namespace {

// The query a chat line's words ask: 0 f_version or !version, 1 f_server, 2 f_fakeshaft, -1 none.
// The word, then only digits and blanks.
int query_of(std::string_view words) {
  std::string plain;
  for (const char ch : words) plain += static_cast<char>(static_cast<unsigned char>(ch) & 0x7f);
  while (!plain.empty() && (plain.back() == '\n' || plain.back() == '\r')) plain.pop_back();
  const auto only_after = [&plain](size_t from) {
    for (size_t i = from; i < plain.size(); ++i)
      if (plain[i] != ' ' && (plain[i] < '0' || plain[i] > '9')) return false;
    return true;
  };
  for (const auto& [word, kind] :
       {std::pair<const char*, int>{"f_version", 0}, std::pair<const char*, int>{"!version", 0},
        std::pair<const char*, int>{"f_server", 1},
        std::pair<const char*, int>{"f_fakeshaft", 2}}) {
    const std::string_view w = word;
    if (plain.starts_with(w) && only_after(w.size())) return kind;
  }
  return -1;
}

// Any of the replies' words in a chat line: such a line fires no trigger.
bool asks_or_answers(std::string_view words) {
  static constexpr const char* kWords[] = {"f_version", "f_skins",   "f_fakeshaft",
                                           "f_server",  "f_scripts", "f_cmdline",
                                           "f_system",  "f_speed",   "f_modified"};
  for (const char* w : kWords)
    if (words.find(w) != std::string_view::npos) return true;
  return false;
}

}  // namespace

void chat_line_teamplay(ChatExtras& x, DemoState& s, const con::ChatLine& line, DemoPrint& p,
                        double now_s, bool triggers_on, std::string_view version) {
  const bool chat = p.level == kChatLevel;
  const size_t colon = chat && line.place >= 0 ? p.text.find(": ") : std::string::npos;
  const size_t words_at = colon == std::string::npos ? 0 : colon + 2;
  const bool own_team = line.kind == con::ChatKind::team;
  if (chat && (x.nofake == 1 || (x.nofake == 2 && !own_team))) {
    for (size_t i = words_at; i < p.text.size(); ++i)
      if (p.text[i] == '\r' || (p.text[i] == '\n' && i + 1 < p.text.size())) p.text[i] = ' ';
  }
  const bool sounds_quiet = x.con != nullptr && x.con->sound_triggers_quiet();  // a ruleset's
  if (chat && own_team && !x.sound_trigger.empty() && !sounds_quiet) {
    // The sound word: the line's last run of name characters after a trigger character.
    size_t end = p.text.size();
    while (end > words_at && (p.text[end - 1] == '\n' || p.text[end - 1] == '\r')) --end;
    size_t at = end;
    const auto name_char = [](char ch) {
      const auto u = static_cast<unsigned char>(ch);
      return u > ' ' && u < 127 &&
             std::string_view("\"&'*,:;<>?|").find(ch) == std::string_view::npos;
    };
    while (at > words_at && name_char(p.text[at - 1]) &&
           x.sound_trigger.find(p.text[at - 1]) == std::string::npos)
      --at;
    if (at > words_at && x.sound_trigger.find(p.text[at - 1]) != std::string::npos && at < end &&
        end - at <= 63) {
      std::string name = p.text.substr(at, end - at);
      std::replace(name.begin(), name.end(), '\\', '/');
      p.text.erase(at - 1, end - at + 1);
      if (name.find("..") == std::string::npos) {
        if (name.find('.') == std::string::npos) name += ".wav";
        std::string& kept = x.sound_names[x.sound_next++ % x.sound_names.size()];
        kept = name;
        // In place of the chat sound this line started (chat_line_extras added it last).
        if (!s.started.empty() && s.started.back().name != nullptr &&
            s.started.back().entity == s.player + 1 && s.started.back().attenuation == 0)
          s.started.pop_back();
        DemoSound d;
        d.entity = s.player + 1;
        d.name = kept.c_str();
        d.attenuation = 0;
        s.started.push_back(d);
      }
    }
  }
  const std::string words = p.text.substr(std::min(words_at, p.text.size()));
  const bool asking = chat && asks_or_answers(words);
  const bool watching = s.player >= 0 && static_cast<size_t>(s.player) < s.spectator.size() &&
                        s.spectator[static_cast<size_t>(s.player)];
  const bool spoken = line.kind == con::ChatKind::everyone || line.kind == con::ChatKind::spectator;
  const bool q2 = s.format == DemoFormat::q2;
  if (q2 && chat && x.queries && x.con != nullptr && line.place != s.player && x.reply_due < 0 &&
      now_s - x.answered_at[0] >= kQ2ReplyEveryS) {
    std::string plain;  // the words' end, high bits off, newlines and blanks after dropped
    for (const char ch : words) plain += static_cast<char>(static_cast<unsigned char>(ch) & 0x7f);
    while (!plain.empty() && (plain.back() == '\n' || plain.back() == '\r' || plain.back() == ' '))
      plain.pop_back();
    if (plain.ends_with("!version")) {
      x.answered_at[0] = now_s;
      // 1 to 2 s on, spread by the moment it was asked (no two clients answer as one).
      const double spread = now_s * 1000 - std::floor(now_s * 1000 / 997) * 997;
      x.reply_due = now_s + kQ2ReplyLeastS + (kQ2ReplyMostS - kQ2ReplyLeastS) * spread / 997;
    }
  }
  if (!q2 && chat && x.queries && x.con != nullptr && spoken && line.place != s.player &&
      !watching) {
    const int q = query_of(words);
    if (q >= 0 && now_s - x.answered_at[static_cast<size_t>(q)] >= 20) {
      x.answered_at[static_cast<size_t>(q)] = now_s;
      const std::string server = x.con->expand_text("$cl_server");
      const ge::console::Cvar* shaft = x.con->cvars().find("cl_fakeshaft");
      if (q == 0)
        x.con->append("say \"" + std::string(version) + "\"");
      else if (q == 2 && shaft != nullptr)  // the beam's smoothing, as the setting has it
        x.con->append("say \"" + ge::classic::fakeshaft_words(std::atof(shaft->value.c_str())) +
                      "\"");
      else if (q == 1 && server != "$cl_server" && !server.empty())
        x.con->append("say \"f_server response: " + server + "\"");
    }
  }
  if (triggers_on && x.con != nullptr && !asking) {
    const int level = p.level < 0 ? 0 : p.level > kChatLevel ? kChatLevel : p.level;
    x.con->fire_triggers(words, level, own_team);
  }
}

void chat_reply_due(ChatExtras& x, double now_s, std::string_view version) {
  if (x.reply_due < 0 || now_s < x.reply_due) return;
  x.reply_due = -1;
  if (x.con != nullptr) x.con->append("say \"" + std::string(version) + "\"");
}

}  // namespace shooters_edge::client
