// COOP SETTINGS every classic game's coop shares, so both games mean one thing by each name and
// start from one default. Each game's server registers them from this table and plays them in its
// own rules:
//
// - `coop_pickup_weapons`: in coop a weapon on the map stays for everyone, and each player takes
//   it once (a player who holds it is refused). With 1 a holder may still take a copy nobody has
//   taken yet, for its ammunition; once someone has taken that copy, holders are refused again.
// - `coop_elevator_delay`: in coop a platform stepped on at the bottom waits this many seconds
//   before it rises, so the others can board.
//
// A loaded mod's own rules play coop as the mod does; these are our rules' settings.
//
// ge:summary The coop settings both classic games share: names, defaults and their words
// ge:keywords coop coop_pickup_weapons coop_elevator_delay weapons-stay plat-delay
// ge:keywords coop-settings classic-layer
// ge:skill classic shooter
// ge:test tests/shooters_edge_campaign_test.cpp
#pragma once

namespace ge::classic {

struct CoopSetting {
  const char* name;
  const char* fallback;
  const char* help;
  const char* values;
};

inline constexpr CoopSetting kCoopSettings[] = {
    {"coop_pickup_weapons", "1",
     "In coop, whether a player who holds a weapon may still take a copy of it nobody has taken "
     "yet, for its ammunition (weapons stay in coop).",
     "0: never; 1: once a copy (the default)"},
    {"coop_elevator_delay", "1.0",
     "In coop, the seconds a platform waits after it is stepped on before it rises, so the "
     "others can board.",
     "0 or less: it rises at once; N: that many seconds (1 the default)"},
};

}  // namespace ge::classic
