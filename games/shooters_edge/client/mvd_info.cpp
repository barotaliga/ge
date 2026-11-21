// The demo watcher's table of every player and the player picked by name (mvd_info.hpp).
//
// ge:summary The demo watcher's table of every player (mvd_info) and the player picked by name
// ge:keywords mvd_info_lines plain_track_name name_track_slot
// ge:skill shooter replay ui
// ge:test tests/shooters_edge_mvd_info_test.cpp
#include "shooters_edge/client/mvd_info.hpp"

#include <algorithm>
#include <cstdio>

#include "shooters_edge/client/demo_autotrack.hpp"
#include "shooters_edge/client/team_chat.hpp"
#include "shooters_edge/client/timedemo.hpp"

namespace shooters_edge::client {

namespace {

constexpr int kSlots = 32;

std::string header_word(char code) {
  switch (code) {
    case 'a':
      return "Armor";
    case 'f':
      return "Frags";
    case 'h':
      return "Health";
    case 'l':
      return "Location";
    case 'n':
      return "Nick";
    case 'P':
      return "Ping";
    case 'p':
      return "Powerup";
    case 'v':
      return "Value";
    case 'w':
      return "Cur.Weap.";
    case 'W':
      return "Best Weap.";
    default:
      return {};
  }
}

bool known_code(char c) { return !header_word(c).empty(); }

// `setup` with each code given by `fill`, padded to its width.
template <typename Fill>
std::string fill_row(const std::string& setup, const Fill& fill) {
  std::string out;
  for (size_t i = 0; i < setup.size(); i++) {
    if (setup[i] != '%') {
      out += setup[i];
      continue;
    }
    size_t j = i + 1;
    const bool left = j < setup.size() && setup[j] == '-';
    if (left) j++;
    size_t width = 0;
    while (j < setup.size() && setup[j] >= '0' && setup[j] <= '9')
      width = width * 10 + static_cast<size_t>(setup[j++] - '0');
    if (j >= setup.size()) break;  // a code with nothing after ends the row
    const char code = setup[j];
    i = j;
    if (!known_code(code)) continue;
    const std::string v = fill(code);
    const size_t pad = v.size() < width ? width - v.size() : 0;
    if (!left) out.append(pad, ' ');
    out += v;
    if (left) out.append(pad, ' ');
  }
  return out;
}

}  // namespace

std::vector<MvdInfoLine> mvd_info_lines(const Demo& d, const std::string& setup, bool header,
                                        const ge::console::Console& con, const PlaceNames* places) {
  std::vector<MvdInfoLine> out;
  const DemoState& s = d.state;
  if (s.format != DemoFormat::multiview || s.intermission) return out;
  if (header) out.push_back({fill_row(setup, header_word), true});
  for (int i = 0; i < kSlots; i++) {
    const size_t k = static_cast<size_t>(i);
    if (s.names[k].empty() || s.spectator[k]) continue;
    DemoState one;  // the stats the codes read: this player's own
    if (k < d.player_stats.size())
      std::copy(d.player_stats[k].begin(), d.player_stats[k].end(), one.stats);
    TeamChat chat;
    chat.status = demo_status(one, s.time);
    const ge::shooter::Vec3f at =
        k < s.players.size() ? s.players[k].origin : ge::shooter::Vec3f{0, 0, 0};
    const auto fill = [&](char code) -> std::string {
      switch (code) {
        case 'n':
          return s.names[k];
        case 'h':
          return std::to_string(one.stats[kStatHealth]);
        case 'a':
          return std::to_string(one.stats[kStatArmour]);
        case 'f':
          return std::to_string(s.frags[k]);
        case 'l':
          return places != nullptr ? place_at(*places, at) : fill_team_codes(con, chat, "%l");
        case 'w':
          return fill_team_codes(con, chat, "%w");
        case 'W':
          return fill_team_codes(con, chat, "%b");
        case 'v': {
          char buf[32];
          std::snprintf(buf, sizeof buf, "%f",
                        autotrack_kit(one.stats[kStatItems], one.stats[kStatArmour], s.frags[k]));
          return buf;
        }
        default:
          return {};
      }
    };
    out.push_back({fill_row(setup, fill), false});
  }
  return out;
}

std::string plain_track_name(std::string_view name) {
  std::string out;
  for (size_t i = 0; i < name.size(); i++) {
    char c = static_cast<char>(static_cast<unsigned char>(name[i]) & 0x7f);  // red half: white
    if (c == '&' && i + 1 < name.size() && name[i + 1] == 'c' && i + 4 < name.size()) {
      i += 4;  // a colour: &c and three hex digits
      continue;
    }
    if (c == '&' && i + 1 < name.size() && name[i + 1] == 'r') {
      i += 1;
      continue;
    }
    if (c == ' ' || c == '/' || c == '\\' || c == '"' || c == ';' || c == '\n' || c == '\r')
      c = '_';
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    out += c;
  }
  const size_t a = out.find_first_not_of('_');
  if (a == std::string::npos) return {};
  return out.substr(a, out.find_last_not_of('_') - a + 1);
}

int name_track_slot(const DemoState& s, std::string_view wanted) {
  if (wanted.empty()) return -1;
  int best = -1;
  size_t best_at = std::string::npos;
  for (int i = 0; i < kSlots; i++) {
    const size_t k = static_cast<size_t>(i);
    if (s.names[k].empty() || s.spectator[k]) continue;
    const size_t at = plain_track_name(s.names[k]).find(wanted);
    if (at != std::string::npos && (best < 0 || at < best_at)) {
      best = i;
      best_at = at;
    }
  }
  return best;
}

}  // namespace shooters_edge::client
