// Team chat and the game's events in a player's config (team_chat.hpp).
//
// ge:summary Team chat's percent codes, fun characters and f_ event triggers, defined
// ge:keywords say say_team percent-codes fun-characters f_newmap f_spawn f_death f_respawn
// ge:skill shooter console
// ge:test tests/shooters_edge_team_chat_test.cpp
#include "shooters_edge/client/team_chat.hpp"

#include "console/cfg.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <utility>

namespace shooters_edge::client {

namespace {

namespace con = ge::console;

// Glyphs of the classic 16x16 font sheet (team-messages facts, porting/).
constexpr char kGoldOpen = 0x10, kGoldClose = 0x11;
constexpr double kDeathPlace_s = 5;  // %L names where the player died for this long

struct Word {
  const char* name;
  const char* fallback;
  const char* help;
};

constexpr Word kWords[] = {
    {"tp_name_axe", "axe", "the melee weapon in a team message"},
    {"tp_name_sg", "sg", "the shotgun in a team message"},
    {"tp_name_ssg", "ssg", "the double shotgun in a team message"},
    {"tp_name_ng", "ng", "the nailgun in a team message"},
    {"tp_name_sng", "sng", "the heavy nailgun in a team message"},
    {"tp_name_gl", "gl", "the grenade launcher in a team message"},
    {"tp_name_rl", "rl", "the rocket launcher in a team message"},
    {"tp_name_lg", "lg", "the beam gun in a team message"},
    {"tp_name_armortype_ga", "g", "light armour's letter (%A)"},
    {"tp_name_armortype_ya", "y", "medium armour's letter (%A)"},
    {"tp_name_armortype_ra", "r", "heavy armour's letter (%A)"},
    {"tp_name_quad", "quad", "the damage boost in a team message"},
    {"tp_name_pent", "pent", "the shield in a team message"},
    {"tp_name_ring", "ring", "the cloak in a team message"},
    {"tp_name_suit", "suit", "the suit in a team message"},
    {"tp_name_ga", "ga", "light armour picked up"},
    {"tp_name_ya", "ya", "medium armour picked up"},
    {"tp_name_ra", "ra", "heavy armour picked up"},
    {"tp_name_mh", "mega", "the big health picked up"},
    {"tp_name_backpack", "pack", "a backpack picked up"},
    {"tp_tooktimeout", "15", "seconds a pickup is remembered for %i, %Y and tp_msgtook"},
    {"tp_name_rune1", "resistance", "the first rune"},
    {"tp_name_rune2", "strength", "the second rune"},
    {"tp_name_rune3", "haste", "the third rune"},
    {"tp_name_rune4", "regeneration", "the fourth rune"},
    {"tp_name_shells", "shells", "shells in a team message"},
    {"tp_name_nails", "nails", "nails in a team message"},
    {"tp_name_rockets", "rox", "rockets in a team message"},
    {"tp_name_cells", "cells", "cells in a team message"},
    {"tp_name_armor", "armor", "armour when you need it (%u)"},
    {"tp_name_health", "health", "health when you need it (%u)"},
    {"tp_name_weapon", "weapon", "a weapon when you have none (%u)"},
    {"tp_name_nothing", "nothing", "nothing to report"},
    {"tp_name_at", "at", "the word between a thing and its place"},
    {"tp_name_none", "", "no armour, weapon or powerup"},
    {"tp_name_separator", "/", "between the words of a list (%p, %u)"},
    {"tp_weapon_order", "78654321", "the best weapon first, by number (%b)"},
    {"tp_name_rlg", "rlg", "the rocket launcher and the beam gun together (tp_msgsafe)"},
    {"tp_name_enemy", "enemy", "an enemy in a team message"},
    {"tp_name_teammate", "", "a teammate in a team message (empty: their name)"},
    {"tp_name_eyes", "eyes", "a cloaked player's eyes in a team message"},
    {"tp_name_quaded", "quaded", "a player with the damage boost"},
    {"tp_name_pented", "pented", "a player with the shield"},
    {"tp_poweruptextstyle", "0", "1: the powerups you hold as their letters in team messages"},
    {"cl_fakename_suffix", ": ", "after your short name in a team message"},
};

struct Need {
  const char* name;
  const char* fallback;
};

constexpr Need kNeeds[] = {
    {"tp_need_ga", "60"},     {"tp_need_ya", "80"},     {"tp_need_ra", "120"},
    {"tp_need_health", "50"}, {"tp_need_weapon", "87"}, {"tp_need_rl", "1"},
    {"tp_need_shells", "0"},  {"tp_need_nails", "0"},   {"tp_need_rockets", "5"},
    {"tp_need_cells", "13"},
};

std::string word(const con::Console& c, std::string_view name) {
  const con::Cvar* v = c.cvars().find(name);
  return v == nullptr ? std::string() : v->value;
}

double number(const con::Console& c, std::string_view name) {
  const con::Cvar* v = c.cvars().find(name);
  return v == nullptr ? 0 : v->number;
}

bool on(const con::Console& c, std::string_view name) {
  const con::Cvar* v = c.cvars().find(name);
  return v == nullptr || v->on();
}

// The weapon's number in a config's lists: 1 the melee weapon .. 8 the beam gun.
int weapon_digit(hud::Weapon w) { return static_cast<int>(w) + 1; }

// The word setting naming weapon `digit` ("tp_name_rl"), tp_name_none for none.
const char* weapon_name(int digit) {
  static constexpr const char* kNames[] = {"tp_name_axe", "tp_name_sg",  "tp_name_ssg",
                                           "tp_name_ng",  "tp_name_sng", "tp_name_gl",
                                           "tp_name_rl",  "tp_name_lg"};
  return digit >= 1 && digit <= 8 ? kNames[digit - 1] : "tp_name_none";
}

std::string weapon_word(const con::Console& c, int digit) { return word(c, weapon_name(digit)); }

int ammo_of_digit(const hud::Status& s, int digit) {
  return digit >= 1 && digit <= 8 ? s.ammo_for(static_cast<hud::Weapon>(digit - 1)) : 0;
}

bool owns(const hud::Status& s, int digit) {
  return digit >= 1 && digit <= 8 && s.has(static_cast<hud::Weapon>(digit - 1));
}

std::string armour_word(const con::Console& c, const hud::Status& s) {
  switch (s.armour_class) {
    case hud::ArmourClass::light:
      return word(c, "tp_name_armortype_ga");
    case hud::ArmourClass::medium:
      return word(c, "tp_name_armortype_ya");
    case hud::ArmourClass::heavy:
      return word(c, "tp_name_armortype_ra");
    case hud::ArmourClass::none:
      break;
  }
  return word(c, "tp_name_none");
}

std::string powers(const con::Console& c, const hud::Status& s) {
  std::string out;
  const std::string sep = word(c, "tp_name_separator");
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

std::string best(const con::Console& c, const hud::Status& s) {
  for (const std::string& order : {word(c, "tp_weapon_order"), std::string("78654321")}) {
    for (const char ch : order) {
      const int d = ch - '0';
      if (owns(s, d))
        return weapon_word(c, d) + ":" + std::to_string(d == 1 ? 0 : ammo_of_digit(s, d));
    }
  }
  return word(c, "tp_name_none") + ":0";
}

// What the player needs by the `tp_need_*` thresholds (%u); `ammo` names the kind ("" none).
struct Needed {
  bool armour = false, health = false, weapon = false, rl = false;
  std::string ammo;
  bool any() const { return armour || health || weapon || rl || !ammo.empty(); }
};

Needed needed(const con::Console& c, const hud::Status& s) {
  Needed n;
  const double ga = number(c, "tp_need_ga"), ya = number(c, "tp_need_ya"),
               ra = number(c, "tp_need_ra");
  n.armour = s.armour_class == hud::ArmourClass::light    ? s.armour < ga
             : s.armour_class == hud::ArmourClass::medium ? s.armour < ya
             : s.armour_class == hud::ArmourClass::heavy  ? s.armour < ra
                                                          : (ga != 0 || ya != 0 || ra != 0);
  const double health = number(c, "tp_need_health");
  n.health = health != 0 && s.health < health;
  // The weapon: tp_need_weapon read from its last digit, the first owned deciding.
  const std::string want = word(c, "tp_need_weapon");
  int have = 0;
  for (size_t i = want.size(); i-- > 0;) {
    const int d = want[i] - '0';
    if (d >= 2 && d <= 8 && owns(s, d)) {
      have = d;
      break;
    }
  }
  if (have == 0) {
    n.weapon = true;
  } else {
    n.rl = number(c, "tp_need_rl") != 0 && !owns(s, 7);
    static constexpr const char* kAmmo[] = {"shells",  "shells",  "nails", "nails",
                                            "rockets", "rockets", "cells"};
    const std::string kind = kAmmo[have - 2];
    const double need = number(c, "tp_need_" + kind);
    if (need != 0 && ammo_of_digit(s, have) < need) n.ammo = kind;
  }
  return n;
}

std::string needs(const con::Console& c, const hud::Status& s) {
  std::string out;
  const std::string sep = word(c, "tp_name_separator");
  auto add = [&](const std::string& w) {
    if (!out.empty()) out += sep;
    out += w;
  };
  const Needed n = needed(c, s);
  if (n.armour) add(word(c, "tp_name_armor"));
  if (n.health) add(word(c, "tp_name_health"));
  if (n.weapon) add(word(c, "tp_name_weapon"));
  if (n.rl) add(word(c, "tp_name_rl"));
  if (!n.ammo.empty()) add(word(c, "tp_name_" + n.ammo));
  return out.empty() ? word(c, "tp_name_nothing") : out;
}

std::string here(const con::Console& c, const TeamChat& chat) {
  if (chat.places != nullptr) return fill_place_codes(*chat.places, "%l");
  const std::string w = word(c, "tp_name_someplace");
  return w.empty() ? "someplace" : w;
}

std::string someplace(const con::Console& c) {
  const std::string w = word(c, "tp_name_someplace");
  return w.empty() ? "someplace" : w;
}

// A pickup remembered (`tp_tooktimeout`).
bool took_fresh(const con::Console& c, const TeamChat& chat) {
  return !chat.took.empty() && chat.status.t - chat.took_t <= number(c, "tp_tooktimeout");
}

// What a frame's state says was picked up since the last one: the word setting's name
// ("tp_name_ra"), "" for nothing the team is told of. Read from the stats alone: a powerup's
// time starting, armour at a box's value, a new weapon (the beam gun first), the big health, a
// rune, then rockets, cells or several kinds of ammunition at once (a pack).
std::string picked_up(const hud::Status& was, const hud::Status& now) {
  const std::pair<hud::Power, const char*> kPowers[] = {{hud::Power::damage, "tp_name_quad"},
                                                        {hud::Power::shield, "tp_name_pent"},
                                                        {hud::Power::cloak, "tp_name_ring"},
                                                        {hud::Power::suit, "tp_name_suit"}};
  for (const auto& [p, name] : kPowers)
    if (!was.powered(p) && now.powered(p)) return name;
  if (now.armour > was.armour) {
    if (now.armour == 100) return "tp_name_ga";
    if (now.armour == 150) return "tp_name_ya";
    if (now.armour == 200) return "tp_name_ra";
  }
  for (int d = 8; d >= 3; --d)
    if (!owns(was, d) && owns(now, d)) return weapon_name(d);
  if (now.health > 100 && now.health - was.health > 25) return "tp_name_mh";
  for (int k = 0; k < 4; ++k)
    if ((now.runes & (1u << k)) != 0 && (was.runes & (1u << k)) == 0)
      return k == 0   ? "tp_name_rune1"
             : k == 1 ? "tp_name_rune2"
             : k == 2 ? "tp_name_rune3"
                      : "tp_name_rune4";
  int kinds = 0;
  for (size_t k = 0; k < now.ammo.size(); ++k) kinds += now.ammo[k] > was.ammo[k] ? 1 : 0;
  if (kinds >= 2) return "tp_name_backpack";
  if (now.ammo[2] > was.ammo[2]) return "tp_name_rockets";
  if (now.ammo[3] > was.ammo[3]) return "tp_name_cells";
  return {};
}

// A code's text, false when `code` is not one ("a", "[h]").
bool code_text(const con::Console& c, const TeamChat& chat, const std::string& code,
               std::string& out) {
  const hud::Status& s = chat.status;
  const auto gold = [](const std::string& t, bool low) {
    return low ? std::string(1, kGoldOpen) + t + std::string(1, kGoldClose) : t;
  };
  if (code == "h") {
    out = std::to_string(s.health);
  } else if (code == "a") {
    out = std::to_string(s.armour);
  } else if (code == "A") {
    out = armour_word(c, s);
  } else if (code == "c") {
    out = std::to_string(s.ammo[static_cast<size_t>(hud::Ammo::cells)]);
  } else if (code == "w") {
    const int d = weapon_digit(s.weapon);
    out = weapon_word(c, d) + ":" + std::to_string(ammo_of_digit(s, d));
  } else if (code == "b") {
    out = best(c, s);
  } else if (code == "p" || code == "P") {
    out = powers(c, s);
  } else if (code == "R") {
    out.clear();
    for (int k = 0; k < 4; ++k) {
      if ((s.runes >> k & 1) != 0) {
        out = word(c, "tp_name_rune" + std::to_string(k + 1));
        break;
      }
    }
  } else if (code == "u") {
    out = needs(c, s);
  } else if (code == "l") {
    out = chat.reported = here(c, chat);
  } else if (code == "d") {
    out = chat.died_at.empty() ? someplace(c) : chat.died_at;
    if (!chat.died_at.empty()) chat.reported = out;
  } else if (code == "L") {
    out =
        !chat.died_at.empty() && s.t - chat.died_t <= kDeathPlace_s ? chat.died_at : here(c, chat);
    chat.reported = out;
  } else if (code == "r") {
    out = chat.reported.empty() ? someplace(c) : chat.reported;
  } else if (code == "S") {
    out = word(c, "skin");
  } else if (code == "n" || code == "N") {
    out.clear();
  } else if (code == "i" || code == "X") {
    out = took_fresh(c, chat) ? word(c, chat.took) : word(c, "tp_name_nothing");
    if (code == "i" && took_fresh(c, chat)) out += " " + word(c, "tp_name_at") + " " + chat.took_at;
  } else if (code == "k" || code == "m") {  // took or pointed, whichever is newer, within 5 s
    out = took_fresh(c, chat) && s.t - chat.took_t <= kDeathPlace_s
              ? word(c, chat.took) + " " + word(c, "tp_name_at") + " " + chat.took_at
              : word(c, "tp_name_nothing") + " " + word(c, "tp_name_at") + " " + someplace(c);
  } else if (code == "j" || code == "t" || code == "x") {
    out = word(c, "tp_name_nothing");
  } else if (code == "q") {
    out = word(c, "tp_name_quad");
  } else if (code == "y") {
    out = here(c, chat);
  } else if (code == "Y") {
    out = took_fresh(c, chat) ? chat.took_at : someplace(c);
  } else if (code == "[a]") {
    const std::string type = s.armour_class == hud::ArmourClass::none ? "a" : armour_word(c, s);
    out = gold(type + ":" + std::to_string(s.armour), s.armour < 30);
  } else if (code == "[h]") {
    out = gold(std::to_string(s.health), s.health < 50);
  } else if (code == "[p]" || code == "[P]") {
    const std::string p = powers(c, s);
    out = p.empty() ? p : gold(p, true);
  } else {
    return false;
  }
  return true;
}

// `t` padded or cut to `width` characters by `align` ('l', 'r', 'c').
std::string fit(const std::string& t, size_t width, char align) {
  if (t.size() >= width) return t.substr(0, width);
  const size_t pad = width - t.size();
  if (align == 'r') return std::string(pad, ' ') + t;
  if (align == 'c') return std::string(pad / 2, ' ') + t + std::string(pad - pad / 2, ' ');
  return t + std::string(pad, ' ');
}

int hex(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

// The glyph a fun pair selects, -1 when `ch` after a `$` names none.
int fun_glyph(char ch) {
  switch (ch) {
    case '\\':
      return 0x0D;
    case ':':
      return 0x0A;
    case '[':
      return 0x10;
    case ']':
      return 0x11;
    case 'G':
      return 0x86;
    case 'R':
      return 0x87;
    case 'Y':
      return 0x88;
    case 'B':
      return 0x89;
    case 'W':
      return 0x84;
    case '(':
      return 0x80;
    case '=':
      return 0x81;
    case ')':
      return 0x82;
    case 'a':
      return 0x83;
    case 'b':
      return 0x8B;
    case 'c':
    case 'd':
      return 0x8D;
    case '<':
      return 0x1D;
    case '-':
      return 0x1E;
    case '>':
      return 0x1F;
    case ',':
      return 0x1C;
    case '.':
      return 0x9C;
    case '$':
      return '$';
    case '^':
      return '^';
    default:
      break;
  }
  if (ch >= '0' && ch <= '9') return ch - '0' + 0x12;
  return -1;
}

void run_alias(con::Console& c, std::string_view name) {
  if (c.aliases().count(std::string(name)) != 0) c.append(name);
}

}  // namespace

std::string fill_team_codes(const con::Console& con, const TeamChat& chat, std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '%' || i + 1 >= text.size()) {
      out += text[i];
      continue;
    }
    if (text[i + 1] == '%') {
      out += '%';
      ++i;
      continue;
    }
    size_t at = i + 1;
    size_t width = 0;
    bool sized = false;
    char align = 'l';
    if (text[at] == '<') {  // %<width[lrc]>code
      const size_t close = text.find('>', at);
      bool ok = close != std::string_view::npos && close > at + 1;
      size_t end = close;
      if (ok) {
        const char last = static_cast<char>(text[close - 1] | 0x20);
        if (last == 'l' || last == 'r' || last == 'c') {
          align = last;
          end = close - 1;
        }
        for (size_t k = at + 1; k < end; ++k) ok = ok && text[k] >= '0' && text[k] <= '9';
        ok = ok && end > at + 1;
      }
      if (!ok) {
        out += '%';
        continue;
      }
      for (size_t k = at + 1; k < end; ++k) width = width * 10 + static_cast<size_t>(text[k] - '0');
      sized = true;
      at = close + 1;
    }
    std::string code;
    if (at < text.size() && text[at] == '[' && at + 2 < text.size() && text[at + 2] == ']') {
      code = std::string(text.substr(at, 3));
    } else if (at < text.size()) {
      code = std::string(1, text[at]);
    }
    std::string got;
    if (code.empty() || !code_text(con, chat, code, got)) {
      out += '%';
      continue;
    }
    out += sized ? fit(got, width, align) : got;
    i = at + code.size() - 1;
  }
  return out;
}

std::string fun_characters(std::string_view text, bool chat) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '$' && i + 1 < text.size()) {
      const char next = text[i + 1];
      if (next == 'x') {
        const int hi = i + 2 < text.size() ? hex(text[i + 2]) : -1;
        const int lo = i + 3 < text.size() ? hex(text[i + 3]) : -1;
        if (hi >= 0 && lo >= 0) {
          const int b = hi * 16 + lo;
          out += b == 0 ? ' ' : static_cast<char>(b);
          i += 3;
          continue;
        }
      } else if (const int g = fun_glyph(next); g >= 0) {
        out += static_cast<char>(g);
        ++i;
        continue;
      }
    } else if (!chat && ch == '^' && i + 1 < text.size() && text[i + 1] != ' ') {
      out += static_cast<char>(static_cast<unsigned char>(text[i + 1]) | 0x80u);
      ++i;
      continue;
    }
    out += ch;
  }
  return out;
}

namespace {

// THE CANNED TEAM MESSAGES (`tp_msg*`): each a body sent to the team behind the player's short
// name. What the player points at and took is not followed yet, so the messages that need it act
// as though the player points at nothing and took nothing. `{...}` and `&cRGB` are the markup a
// teammate's client draws white and in colour.

constexpr const char* kTeam = "{&c0b0team&cfff} ";

bool holding(const hud::Status& s, int digit) { return weapon_digit(s.weapon) == digit; }

std::string held_powers(const con::Console& c, const hud::Status& s) {
  const bool letters = number(c, "tp_poweruptextstyle") == 1;
  std::string out;
  if (s.powered(hud::Power::damage)) out += letters ? "{&c03Fq&cfff}" : word(c, "tp_name_quad");
  if (letters) {
    if (s.powered(hud::Power::cloak)) out += "{&cff0r&cfff}";
    if (s.powered(hud::Power::shield)) out += "{&ce00p&cfff}";
  } else {
    if (s.powered(hud::Power::shield)) out += word(c, "tp_name_pent");
    if (s.powered(hud::Power::cloak)) out += word(c, "tp_name_ring");
  }
  return out;
}

bool any_power(const hud::Status& s) {
  return s.powered(hud::Power::damage) || s.powered(hud::Power::shield) ||
         s.powered(hud::Power::cloak);
}

std::string coloured_armour(const hud::Status& s) {
  switch (s.armour_class) {
    case hud::ArmourClass::light:
      return "{&c0b0%a&cfff}";
    case hud::ArmourClass::medium:
      return "{&cff0%a&cfff}";
    case hud::ArmourClass::heavy:
      return "{&ce00%a&cfff}";
    case hud::ArmourClass::none:
      break;
  }
  return "0";
}

std::string count(int n) { return std::to_string(n); }

std::string lost(const con::Console& c, const hud::Status& s) {
  const std::string place = "{&cf00[&cfff}{%d}{&cf00]&cfff} {%E}";
  const bool held = holding(s, 7) || holding(s, 8);
  if (s.health >= 1 || (!s.powered(hud::Power::damage) && !held))
    return "{&cf00lost&cfff} " + place;
  std::string out;
  if (s.powered(hud::Power::damage)) out += word(c, "tp_name_quad") + " over ";
  if (held) out += "{&cf00DROPPED} " + weapon_word(c, weapon_digit(s.weapon)) + " ";
  return out + place;
}

std::string need(const con::Console& c, const hud::Status& s) {
  if (s.health < 1 || !needed(c, s).any()) return {};
  const std::string p = held_powers(c, s);
  return (any_power(s) ? std::string(kTeam) + p + " " : std::string()) + "need %u $[{%l}$]";
}

std::string enemy_power(const con::Console& c, const hud::Status& s) {
  if (any_power(s)) {
    if (s.health < 1) return lost(c, s);
    const Needed n = needed(c, s);
    if (n.health || n.armour || n.rl || n.ammo == "rockets" || n.ammo == "cells") return need(c, s);
    return std::string(kTeam) + held_powers(c, s) + " at $[{%y}$]";
  }
  return word(c, "tp_name_enemy") + " %q";
}

std::string best_name(const con::Console& c, const hud::Status& s) {
  for (const std::string& order : {word(c, "tp_weapon_order"), std::string("78654321")})
    for (const char ch : order)
      if (owns(s, ch - '0')) return weapon_word(c, ch - '0');
  return word(c, "tp_name_none");
}

// The body of `tp_msg<which>` from the player's state, "" when it sends nothing.
std::string canned(const con::Console& c, const TeamChat& chat, std::string_view which) {
  const hud::Status& s = chat.status;
  const std::string p = held_powers(c, s);
  const std::string lead = p.empty() ? std::string() : p + " ";
  const bool rl = owns(s, 7), lg = owns(s, 8);
  const std::string rox = count(s.ammo[2]), cells = count(s.ammo[3]);
  const std::string w_rl = word(c, "tp_name_rl"), w_lg = word(c, "tp_name_lg");
  if (which == "report") {
    if (s.health < 1) return lost(c, s);
    std::string weapons;
    if (rl) weapons += w_rl + ":" + rox + " ";
    if (lg) weapons += w_lg + ":" + cells + " ";
    if (!rl && !lg) {
      if (owns(s, 6))
        weapons = word(c, "tp_name_gl") + ":" + rox + " ";
      else if (owns(s, 5))
        weapons = word(c, "tp_name_sng") + ":" + count(s.ammo[1]) + " ";
      else if (owns(s, 3))
        weapons = word(c, "tp_name_ssg") + ":" + count(s.ammo[0]) + " ";
    }
    std::string out = lead + coloured_armour(s) + "/%h " + weapons + "$[{%l}$]";
    if (s.ammo[2] > 0 && !rl) out += " {&cf13r&cfff}:" + rox;
    if (s.ammo[3] > 0 && !lg) out += " {&c2aac&cfff}:" + cells;
    return out;
  }
  if (which == "lost") return lost(c, s);
  if (which == "enemypwr") return enemy_power(c, s);
  if (which == "quaddead")
    return s.powered(hud::Power::damage) && s.health < 1 ? lost(c, s)
                                                         : word(c, "tp_name_quad") + " dead/over";
  if (which == "getquad" || which == "getpent") {
    const bool quad = which == "getquad";
    if (s.powered(quad ? hud::Power::damage : hud::Power::shield)) return enemy_power(c, s);
    return "get " + word(c, quad ? "tp_name_quad" : "tp_name_pent");
  }
  if (which == "took") {
    if (!took_fresh(c, chat)) return {};
    const std::string& t = chat.took;
    if (t == "tp_name_quad" || t == "tp_name_pent" || t == "tp_name_ring") {
      if (s.health < 1) return canned(c, chat, "quaddead");
      const Needed n = needed(c, s);
      const bool short_of =
          n.health || n.armour || n.rl || n.ammo == "rockets" || n.ammo == "cells";
      if (short_of && any_power(s)) return std::string(kTeam) + p + " need %u $[{%Y}$]";
      if (s.powered(hud::Power::damage) || s.powered(hud::Power::cloak))
        return std::string(kTeam) + p + " $[{%Y}$]";
      return "need %u $[{%Y}$]";
    }
    static constexpr const char* kPlain[] = {
        "tp_name_rl",    "tp_name_lg",      "tp_name_gl",    "tp_name_sng",   "tp_name_backpack",
        "tp_name_cells", "tp_name_rockets", "tp_name_mh",    "tp_name_ra",    "tp_name_ya",
        "tp_name_ga",    "tp_name_rune1",   "tp_name_rune2", "tp_name_rune3", "tp_name_rune4"};
    bool plain = false;
    for (const char* k : kPlain) plain = plain || t == k;
    const std::string item = plain ? word(c, t) : "{" + word(c, t) + "}";
    return lead + "took " + item + " $[{%Y}$]";
  }
  if (which == "point") return {};  // nothing pointed at yet
  if (which == "need") return need(c, s);
  if (which == "safe") {
    if (s.health < 1) return {};
    std::string out = "{&c0b0safe&cfff} {&c0b0[&cfff}{%l}{&c0b0]&cfff} ";
    const bool armour = s.armour > 0;
    if (armour) out += coloured_armour(s);
    if (armour && (rl || lg)) out += word(c, "tp_name_separator");
    if (rl && lg)
      out += word(c, "tp_name_rlg");
    else if (rl || lg)
      out += best_name(c, s);
    return out;
  }
  if (which == "killme") {
    if (s.health < 1) return {};
    std::string out = "{&cb1akill me [&cfff}{%l}{&cf2a]&cfff} ";
    if (holding(s, 7)) out += w_rl + ":" + rox + " ";
    if (holding(s, 8)) out += w_lg + ":" + cells + " ";
    if (!holding(s, 7) && s.ammo[2] > 0) out += "{&cf13r&cfff}:" + rox + " ";
    if (!holding(s, 8) && s.ammo[3] > 0) out += "{&c2aac&cfff}:" + cells;
    return out;
  }
  if (which == "utake") return "you take $[{%l}$]";
  if (which == "help") return lead + "{&cff0help&cfff} {&cff0[&cfff}{%l}{&cff0]&cfff} {%e}";
  static constexpr std::pair<const char*, const char*> kOneLine[] = {
      {"yesok", "{yes/ok}"},        {"nocancel", "{&cf00no/cancel&cfff}"},
      {"itemsoon", "item soon"},    {"waiting", "waiting"},
      {"slipped", "enemy slipped"}, {"replace", "replace"},
      {"trick", "trick"},           {"coming", "coming"}};
  for (const auto& [name, body] : kOneLine)
    if (which == name) return lead + body + " $[{%l}$]";
  return {};
}

constexpr const char* kCanned[] = {"report",  "lost",  "enemypwr", "quaddead", "getquad", "getpent",
                                   "took",    "point", "need",     "safe",     "killme",  "utake",
                                   "help",    "yesok", "nocancel", "itemsoon", "waiting", "slipped",
                                   "replace", "trick", "coming"};

// The player's short name before a canned message: a `nick` they made, else their name's first
// three letters; the leading 0x0D draws it over the `(name): ` a teammate's client writes.
std::string short_name(const con::Console& c) {
  std::string nick = word(c, "nick");
  if (nick.empty()) nick = word(c, "name").substr(0, 3);
  return std::string(1, '\r') + nick + word(c, "cl_fakename_suffix");
}

}  // namespace

std::string team_message(const con::Console& c, const TeamChat& chat, std::string_view which) {
  const std::string body = canned(c, chat, which);
  return body.empty() ? body : short_name(c) + body;
}

void team_event(con::Console& c, std::string_view alias) {
  if (on(c, "tp_triggers")) run_alias(c, alias);
}

void add_team_chat(con::Console& c, TeamChat& chat) {
  auto& v = c.cvars();
  for (const Word& w : kWords)
    if (v.find(w.name) == nullptr)
      v.add({.name = w.name,
             .fallback = w.fallback,
             .flags = con::kArchive,
             .help = w.help,
             .kind = con::CvarKind::text});
  for (const Need& n : kNeeds)
    if (v.find(n.name) == nullptr)
      v.add({.name = n.name,
             .fallback = n.fallback,
             .flags = con::kArchive,
             .help = "below this you need it (%u; 0: never)",
             .kind = n.name == std::string_view("tp_need_weapon") ? con::CvarKind::text
                                                                  : con::CvarKind::number});
  for (const char* b : {"cl_parseSay", "cl_parseFunChars", "tp_triggers"})
    v.add({.name = b,
           .fallback = "1",
           .flags = con::kArchive,
           .help = std::string_view(b) == "cl_parseSay" ? "1: %h and the other codes filled in"
                   : std::string_view(b) == "cl_parseFunChars" ? "1: $[ and the other glyphs made"
                                                               : "1: f_ aliases run on events",
           .kind = con::CvarKind::boolean});
  v.add({.name = "tp_forceTriggers",
         .fallback = "0",
         .flags = con::kArchive,
         .help = "1: f_death and f_respawn run with no teammate in the game",
         .kind = con::CvarKind::boolean});
  for (const char* cmd : {"say", "say_team"})
    c.add_command(
        cmd,
        std::string(cmd) + " <words>: " +
            (std::string_view(cmd) == "say" ? "say it to everyone" : "say it to your team") +
            " (%h, %a, %l and the other codes filled in)",
        [&chat](con::Console& con, const con::Args& a) {
          std::string text = a.rest(0);
          if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
            text = text.substr(1, text.size() - 2);
          if (text.empty()) return;
          if (on(con, "cl_parseSay")) text = fill_team_codes(con, chat, text);
          if (on(con, "cl_parseFunChars")) text = fun_characters(text, true);
          const std::string line = a[0] + " \"" + text + "\"";
          if (!chat.send || !chat.send(line)) con.print("You need to be in a game to talk.");
        });
  for (const char* which : kCanned)
    c.add_command(std::string("tp_msg") + which,
                  std::string("tp_msg") + which + ": tell your team, from what you have and where",
                  [&chat, w = std::string(which)](con::Console& con, const con::Args&) {
                    const std::string line = team_message(con, chat, w);
                    if (!line.empty()) con.append("say_team \"" + line + "\"");
                  });
  // `echo` fills in the codes and the glyphs too, `^c` included.
  c.add_command("echo", "echo <words>: print them, one blank between each (%h and $[ filled in)",
                [&chat](con::Console& con, const con::Args& a) {
                  std::string text;
                  for (size_t i = 1; i < a.size(); ++i) text += (i > 1 ? " " : "") + a[i];
                  if (on(con, "cl_parseSay")) text = fill_team_codes(con, chat, text);
                  con.print(fun_characters(text, false));
                });
  // `cfg_load` runs the player's `f_cfgload` after the config, an alias it defines included.
  if (const auto it = c.commands().find("cfg_load"); it != c.commands().end()) {
    con::CommandFn load = it->second.fn;
    c.add_command("cfg_load", it->second.help, [load](con::Console& con, const con::Args& a) {
      std::string file = a.size() > 1 ? a[1] : std::string(con::kConfigFile);
      if (std::filesystem::path(file).extension().empty()) file += ".cfg";
      std::string text;
      if (con.read_file(file, text)) con.insert("if_exists alias f_cfgload f_cfgload");
      load(con, a);
    });
  }
}

void team_frame(con::Console& c, TeamChat& chat, const hud::Status& status, std::string_view map) {
  const bool fire = on(c, "tp_triggers");
  const bool team =
      fire && (number(c, "tp_forceTriggers") != 0 || (chat.teammates && chat.teammates()));
  if (map.empty()) return;
  if (map != chat.map) {  // a new map: what was remembered goes, f_newmap then its first frame
    chat.map = std::string(map);
    chat.known = false;
    chat.last_weapon = -1;
    chat.died_at.clear();
    chat.died_t = -1e9;
    chat.reported.clear();
    chat.took.clear();
    if (fire) run_alias(c, "f_newmap");
  }
  const bool first = !chat.known;
  const int was_health = chat.last_health;
  const std::string took = first || was_health <= 0 || status.health <= 0
                               ? std::string()
                               : picked_up(chat.status, status);
  chat.status = status;
  if (!took.empty()) {
    chat.took = took;
    chat.took_at = here(c, chat);
    chat.took_t = status.t;
    if (team) run_alias(c, "f_took");
  }
  chat.known = true;
  chat.last_health = status.health;
  if (first) {
    if (fire) run_alias(c, "f_spawn");
  } else if (was_health > 0 && status.health <= 0) {
    chat.died_at = here(c, chat);
    chat.died_t = status.t;
    chat.reported = chat.died_at;
    if (team) run_alias(c, "f_death");
  } else if (was_health <= 0 && status.health > 0) {
    if (team) run_alias(c, "f_respawn");
  }
  const int weapon = static_cast<int>(status.weapon);
  if (weapon != chat.last_weapon) {
    chat.last_weapon = weapon;
    if (fire) run_alias(c, "f_weaponchange");
  }
}

}  // namespace shooters_edge::client
