// A chat line's sound, filter, stamp and mark (chat_lines.hpp).
//
// ge:summary The settings of a chat line's extras and the extras on each kept line
// ge:keywords chat-sound msg_filter con_timestamps con_highlight chat_line_extras
// ge:skill net shooter
// ge:test tests/shooters_edge_chat_ignore_test.cpp
#include "shooters_edge/client/chat_lines.hpp"

#include <ctime>

#include "console/cvar.hpp"
#include "shooters_edge/client/cvar_layers.hpp"

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

constexpr int kChatLevel = 3;
constexpr const char* kTalk = "misc/talk.wav";

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

bool chat_line_extras(const ChatExtras& x, DemoState& s, const con::ChatLine& line, DemoPrint& p,
                      const ChatClock& now) {
  const bool watching = s.player >= 0 && static_cast<size_t>(s.player) < s.spectator.size() &&
                        s.spectator[static_cast<size_t>(s.player)];
  const std::string me = s.player >= 0 && static_cast<size_t>(s.player) < s.names.size()
                             ? s.names[static_cast<size_t>(s.player)]
                             : std::string();
  if (!me.empty() && x.highlight >= 2 && p.text.find(me, 2) != std::string::npos)
    p.text = x.mark + p.text;
  if (p.level != kChatLevel) return true;
  const int kind = kind_of(line);
  if (((x.filter & 1) != 0 && (kind == kChatFirst || kind == kChatSpec)) ||
      ((x.filter & 2) != 0 && kind == kChatTeam))
    return false;
  if (x.timestamps != 0) {
    const bool seconds = x.timestamps == 2 && (watching || lower_is(info(s, "status"), "standby"));
    p.text = two(now.hour, ' ') + ":" + two(now.minute, '0') +
             (seconds ? ":" + two(now.second, '0') : std::string()) + " " + p.text;
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

}  // namespace shooters_edge::client
