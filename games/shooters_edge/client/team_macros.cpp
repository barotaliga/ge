// The team's $ macros (team_macros.hpp).
//
// ge:summary The team's $ macros defined over team chat's codes and the player's state
// ge:keywords team-macros dollar-macros ledpoint ledstatus bestweapon weapons tp_name_status
// ge:skill shooter console
// ge:test tests/shooters_edge_team_chat_test.cpp
#include "shooters_edge/client/team_macros.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace shooters_edge::client {

namespace {

namespace con = ge::console;

std::string word(const con::Console& c, std::string_view name) {
  const con::Cvar* v = c.cvars().find(name);
  return v == nullptr ? std::string() : v->value;
}

struct Light {
  const char* name;
  const char* fallback;
  const char* help;
};

constexpr Light kLights[] = {
    {"tp_name_status_green", "$G", "the green light ($ledpoint, $ledstatus, %s)"},
    {"tp_name_status_yellow", "$Y", "the yellow light ($ledpoint, $ledstatus, %s)"},
    {"tp_name_status_red", "$R", "the red light ($ledpoint, $ledstatus, %s)"},
    {"tp_name_status_blue", "$B", "the blue light ($ledpoint)"},
    {"tp_name_status_white", "$W", "the white light"},
    {"tp_name_flag", "flag", "a flag in a team message"},
};

// The powerups held by their letters: q, r, p in the order the older client wrote them.
std::string power_letters(const hud::Status& s) {
  const bool q = s.powered(hud::Power::damage), r = s.powered(hud::Power::cloak),
             p = s.powered(hud::Power::shield);
  std::string out;
  if (q) out += 'q';
  if (r) out += 'r';
  if (p) out += 'p';
  return out;
}

// The powerups held, each by its word, joined by `sep`.
std::string power_words(const con::Console& c, const hud::Status& s, const std::string& sep) {
  std::string out;
  const std::pair<hud::Power, const char*> kOrder[] = {{hud::Power::damage, "tp_name_quad"},
                                                       {hud::Power::shield, "tp_name_pent"},
                                                       {hud::Power::cloak, "tp_name_ring"}};
  for (const auto& [p, name] : kOrder) {
    if (!s.powered(p)) continue;
    if (!out.empty()) out += sep;
    out += word(c, name);
  }
  return out;
}

}  // namespace

void add_team_macros(con::Console& con, TeamChat& chat) {
  con::Cvars& v = con.cvars();
  con.set_macro_widths("tp_length_", "tp_align_");  // `set tp_length_health 3`: a fixed column
  for (const Light& l : kLights)
    if (v.find(l.name) == nullptr)
      v.add({.name = l.name,
             .fallback = l.fallback,
             .flags = con::kArchive,
             .help = l.help,
             .kind = con::CvarKind::text});
  const TeamChat* ch = &chat;
  const auto code = [&con, ch](const char* text) {
    return [&con, ch, text] { return fill_team_codes(con, *ch, text); };
  };
  const auto figure = [ch](int hud::Status::*field) {
    return [ch, field] { return std::to_string(ch->status.*field); };
  };
  const auto ammo = [ch](hud::Ammo a) {
    return [ch, a] { return std::to_string(ch->status.ammo[static_cast<size_t>(a)]); };
  };
  con.add_macro("health", figure(&hud::Status::health));
  con.add_macro("armor", figure(&hud::Status::armour));
  con.add_macro("shells", ammo(hud::Ammo::shells));
  con.add_macro("nails", ammo(hud::Ammo::nails));
  con.add_macro("rockets", ammo(hud::Ammo::rockets));
  con.add_macro("cells", ammo(hud::Ammo::cells));
  con.add_macro("ammo", [ch] {
    const hud::Status& s = ch->status;
    if (s.keyed.set)
      return std::to_string(std::max(0, s.keyed.ammo[static_cast<size_t>(s.keyed.in_hand & 31)]));
    return std::to_string(s.ammo_for(s.weapon));
  });
  con.add_macro("armortype", code("%A"));
  con.add_macro("colored_armor", [ch] {
    return ch->status.armour_class == hud::ArmourClass::none ? std::string("0")
                                                             : std::to_string(ch->status.armour);
  });
  con.add_macro("weapon", [&con, ch] {
    int n = 0;
    const std::string w = hand_weapon_word(con, ch->status, n);
    return w.empty() ? word(con, "tp_name_none") : w;
  });
  con.add_macro("weaponnum", [&con, ch] {
    int n = 0;
    hand_weapon_word(con, ch->status, n);
    return std::to_string(n);
  });
  con.add_macro("weapons", [&con, ch] {
    std::string out;
    const std::string sep = word(con, "tp_name_separator");
    for (const std::string& w : held_weapon_words(con, ch->status)) {
      if (!out.empty()) out += sep;
      out += w;
    }
    return out.empty() ? word(con, "tp_name_none") : out;
  });
  // %b is "word:ammo"; the two halves apart.
  con.add_macro("bestweapon", [&con, ch] {
    const std::string b = fill_team_codes(con, *ch, "%b");
    return b.substr(0, b.rfind(':'));
  });
  con.add_macro("bestammo", [&con, ch] {
    const std::string b = fill_team_codes(con, *ch, "%b");
    const size_t at = b.rfind(':');
    return at == std::string::npos ? std::string("0") : b.substr(at + 1);
  });
  con.add_macro("powerups", [&con, ch] {
    const std::string p = power_words(con, ch->status, word(con, "tp_name_separator"));
    return p.empty() ? word(con, "tp_name_none") : p;
  });
  con.add_macro("colored_powerups", [&con, ch] { return power_words(con, ch->status, ""); });
  con.add_macro("colored_short_powerups", [ch] { return power_letters(ch->status); });
  con.add_macro("tp_powerups", [&con, ch] {
    const con::Cvar* style = con.cvars().find("tp_poweruptextstyle");
    return style != nullptr && style->number != 0 ? power_letters(ch->status)
                                                  : power_words(con, ch->status, "");
  });
  con.add_macro("need", code("%u"));
  con.add_macro("location", code("%l"));
  con.add_macro("deathloc", code("%d"));
  con.add_macro("lastloc", code("%L"));
  con.add_macro("took", code("%X"));
  con.add_macro("tookloc", code("%Y"));
  con.add_macro("tookatloc", code("%i"));
  con.add_macro("point", code("%x"));
  con.add_macro("pointloc", code("%y"));
  con.add_macro("pointatloc", code("%j"));
  // The match: $matchstatus disconnected, standby (the server's status) or normal; $team1 and
  // $team2 the first two teams by name ("team1", "team2" when there are not so many).
  con.add_macro("matchstatus", [ch] {
    return !ch->joined                     ? std::string("disconnected")
           : ch->match_status == "standby" ? std::string("standby")
                                           : std::string("normal");
  });
  con.add_macro("team1", [ch] { return ch->joined ? ch->team1 : std::string("team1"); });
  con.add_macro("team2", [ch] { return ch->joined ? ch->team2 : std::string("team2"); });
  con.add_macro("ledpoint", [&con, ch] {
    fill_team_codes(con, *ch, "%x");  // works out this frame's point
    const Pointed& p = ch->pointed;
    const bool power =
        p.any && (p.item == "tp_name_quad" || p.item == "tp_name_pent" ||
                  p.item == "tp_name_ring" || p.item == "tp_name_suit" || p.item == "tp_name_flag");
    return word(con, p.any && p.enemy      ? "tp_name_status_red"
                     : p.any && p.teammate ? "tp_name_status_green"
                     : power               ? "tp_name_status_yellow"
                                           : "tp_name_status_blue");
  });
  con.add_macro("ledstatus", [&con, ch] {
    const int count = needed_count(con, ch->status, false);
    return word(con, count == 0   ? "tp_name_status_green"
                     : count == 1 ? "tp_name_status_yellow"
                                  : "tp_name_status_red");
  });
}

}  // namespace shooters_edge::client
