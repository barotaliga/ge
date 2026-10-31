// THE CLASSIC TEXT SAVE (porting/shooters-edge/campaign.md § Saves): the campaign written as, and
// read from, the text save players bring from other engines (`<name>.sav`). It keeps less than
// our own save (server/campaign_save.hpp): what the program knows of the game, not the loop's or
// the server's bookkeeping, so a game loaded from it plays on as the program's state says, not
// bit for bit.
//
// THE TEXT, one item a line, then the entity text:
//   5                        the version
//   <comment>                the map's title and the kills ("<kills>/<total>"), spaces as `_`
//   <parm1> ... <parm16>     the player's 16 spawn parms, one a line
//   <skill>                  a whole number
//   <map>                    the map's name
//   <time>                   the server's time, s
//   <style 0> ... <style 63> the 64 light styles, `m` for one not set
//   { "name" "value" ... }   the program's saved globals (those its file marks saved) by name
//   { ... } ...              one block per entity slot from the world on, its non-zero fields by
//                            name (a vector's three parts by the vector's name alone); a free
//                            slot an empty block
// Values are written as the map's entity text reads them (progs/entity_text.hpp's `value_text`).
//
// A LOAD: the map started again (its spawn functions run; `skill` from the text, deathmatch and
// coop off), every entity slot but the world freed, then each block written into its slot: the
// world's fields cleared and rebuilt, an empty block a free slot, any other block a fresh entity
// with its fields; then the globals, the loop's fields from the program's, every entity linked
// into the world again (no touches; one whose box was never placed, its absmin and absmax zero,
// stays out), the time and the light styles set, and the player in his
// slot with the text's parms and nothing run (his entity is as saved). The campaign's
// `serverflags` follow the program's global. A save from an engine that numbers the map's entities
// right after one player (a block from 2 on that is not a player's) is moved onto this server's
// numbering, its entity values with it.
//
// ge:summary The classic text save: the campaign written and read as another engine's .sav
// ge:keywords classic-save sav text-save savegame classic_save_text classic_load_text globals
// ge:keywords entity-blocks light-styles spawn-parms version-5
// ge:skill shooter progs save
// ge:test tests/shooters_edge_classic_save_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "shooters_edge/server/campaign.hpp"

namespace shooters_edge::server {

inline constexpr int kClassicSaveVersion = 5;

// The campaign as the classic text save into `out`. False with `error` when it cannot be saved
// (campaign_save.hpp's refusals). The program's copy of the world is brought up to the loop's
// first (what the next program call would do).
bool classic_save_text(const Campaign& c, std::string& out, std::string& error);

// The classic text save `text` (named `name` in a refusal) loaded into `c` for `progs` with
// `opt`, the player `player` in slot 0. False with `error` (the line and what is wrong) when it
// does not read; `c` then has no server.
bool classic_load_text(Campaign& c, const pg::Progs& progs, const CampaignOptions& opt,
                       std::string_view text, std::string_view name, const std::string& player,
                       std::string& error);

}  // namespace shooters_edge::server
