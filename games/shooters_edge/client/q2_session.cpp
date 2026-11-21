// Quake II's session settings: the map commands, the clock and the state (q2_session.hpp).
//
// ge:summary Quake II's map commands run on a change, its game clock, its read-only state cvars
// ge:keywords q2 cl_beginmapcmd cl_changemapcmd cl_disconnectcmd timescale fixedtime cl_paused
// ge:skill console shooter
// ge:test tests/shooters_edge_q2_session_test.cpp
#include "shooters_edge/client/q2_session.hpp"

#include <algorithm>

#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

constexpr const char* kStateNames[4] = {"cl_running", "cl_paused", "sv_running", "sv_paused"};

void run(con::Console& c, const std::string& text) {
  if (text.find_first_not_of(" \t") != std::string::npos) c.append(text, con::Source::local);
}

}  // namespace

void add_q2_session_cvars(con::Cvars& v, Q2Session& s) {
  const struct {
    const char* name;
    const char* help;
    const char* values;
    std::string* field;
  } commands[] = {
      {"cl_beginmapcmd", "Commands run each time you enter a map, such as record.",
       "\"\": none (the default); TEXT: the commands, ; between them", &s.begin_cmd},
      {"cl_changemapcmd", "Commands run each time the game leaves a map for the next.",
       "\"\": none (the default); TEXT: the commands, ; between them", &s.change_cmd},
      {"cl_disconnectcmd", "Commands run each time you leave a game.",
       "\"\": none (the default); TEXT: the commands, ; between them", &s.disconnect_cmd},
  };
  for (const auto& r : commands)
    v.bind({.name = r.name,
            .fallback = "",
            .flags = con::kArchive,
            .help = r.help,
            .kind = con::CvarKind::text,
            .values = r.values},
           r.field);
  v.bind({.name = "timescale",
          .fallback = "1",
          .flags = con::kCheat,
          .help = "How fast a game you host runs.",
          .min = 0,
          .max = 1000,
          .values = "1: as it should (the default); 0..1000: that many times as fast, only while "
                    "the server allows cheats"},
         &s.timescale);
  v.bind({.name = "fixedtime",
          .fallback = "0",
          .flags = con::kCheat,
          .help = "How far a game you host moves each frame, in milliseconds.",
          .min = 0,
          .max = 1000,
          .values = "0: as far as the time that passed (the default); 1..1000: that many "
                    "milliseconds a frame, only while the server allows cheats"},
         &s.fixed_ms);
  v.bind({.name = "cl_autopause",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether a game you play alone waits while the console or a menu is up.",
          .kind = con::CvarKind::boolean,
          .values = "1: it waits (the default); 0: it runs on"},
         &s.autopause);
  const char* helps[4] = {
      "Whether the game is running: always 1.", "Whether the game you play is paused.",
      "Whether this window runs a server.", "Whether the server this window runs is paused."};
  for (int k = 0; k < 4; ++k)
    v.add({.name = kStateNames[k],
           .fallback = k == 0 ? "1" : "0",
           .flags = con::kReadOnly,
           .help = helps[k],
           .kind = con::CvarKind::boolean,
           .values = "0: no; 1: yes (set by the game alone)"});
  s.state = -1;
}

double q2_game_dt(const Q2Session& s, double host_framerate, double wall) {
  const double step = host_framerate > 0 ? host_framerate
                      : s.fixed_ms > 0   ? s.fixed_ms / 1000
                                         : wall;
  return step * std::max(0.0, s.timescale);
}

void q2_session_step(con::Console& c, Q2Session& s, bool joined, std::string_view map, bool hosting,
                     bool paused) {
  if (!joined) {
    if (s.joined) run(c, s.disconnect_cmd);
    s.joined = false;
    s.map.clear();
  } else {
    if (s.joined && !s.map.empty() && map != s.map) run(c, s.change_cmd);
    if (!map.empty() && map != s.map) run(c, s.begin_cmd);
    s.joined = true;
    s.map = std::string(map);
  }
  const int state = 1 | (paused ? 2 : 0) | (hosting ? 4 : 0) | (hosting && paused ? 8 : 0);
  if (state == s.state) return;
  s.state = state;
  for (int k = 0; k < 4; ++k)
    if (c.cvars().find(kStateNames[k]) != nullptr)
      c.cvars().set(kStateNames[k], (state & (1 << k)) != 0 ? "1" : "0", con::Source::code);
}

void q2_session_frame(con::Console& c, Q2Session& s, const net::Remote& remote,
                      const Listen& listen, bool paused) {
  const LoadedMap* hosted = listen.server != nullptr ? listen.map.get() : nullptr;
  const bool hosting = hosted != nullptr;
  const std::string& map = remote.active       ? remote.demo.state.map
                           : hosted != nullptr ? hosted->name
                                               : s.map;
  q2_session_step(c, s, remote.active || hosting, remote.active || hosting ? map : "", hosting,
                  hosting && (paused || listen.server->paused));
}

}  // namespace shooters_edge::client
