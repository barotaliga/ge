# Settings and commands: how a game's cvars are ported

How a classic game's console settings and commands come over: the census of what the game has, the
gaps against what we have, the rounds that build them gameplay first, and the name check before
each round. The rules a name follows (layers, one name one meaning, synonyms, `same` and `own`, one
config for the series, a game's own value over the common one) are `docs/porting-games.md` § Placing
a setting or command and § One name, one meaning, and `docs/console.md` § Layers; this page is the
procedure.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/console/cvar.hpp` | Cvars in layers (`CvarLayer`, `Overlap::same` and `own`), synonyms (`Cvars::add_synonym`), clashes recorded, `drop_layer` |
| Engine | `engine/console/console.hpp` | Commands, their synonyms (`Console::add_command_synonym`), aliases, exec |
| Engine | `engine/console/cfg.hpp`, `cfg_tidy.hpp`, `catalog.hpp`, `words.hpp` | Startup order, saving one part, scopes (`<game>:name`), tidy configs, the catalog, each setting's words |
| Engine | `engine/server/config.hpp` | The server's settings in layers (`server.md`) |
| Shooter kit | `plugins/shooter/settings.hpp`, `controls.hpp`, `mouse_feel.hpp` | The shooter's shared settings |
| Engine | `engine/console/ruleset.hpp` | Rulesets: settings locked or kept in a range and commands refused under a named ruleset (`Cvars::lock`, `Console::gate_command`) |
| Classic | `plugins/classic/rulesets.hpp` | Each classic game's book of competition rulesets, the ruleset in user info (`*rsn`, `rsn`), the `f_ruleset` answer |
| Shell | `games/shooters_edge/net/qw_remote_rulesets.hpp` | The ruleset on a server joined: the book by game, locked while joined, the answers said |
| Shell | `games/shooters_edge/client/cvar_layers.cpp` | `add_first`, `add_q2`: each game's layer and `GameBindings` |
| Test | `tests/shooters_edge_cvar_layers_test.cpp` | Fails on any undeclared meeting of two layers, for every game |

## Adding a game: the procedure

1. **The census.** A clean-room reader lists the game's client settings and commands (name,
   default, range, what it does, saved or not) into `porting/shooters-edge/<game>-client-inventory.md`,
   and its server's into `<game>-server-inventory.md`. A fork that matters gets a delta note of its
   own (`q2-client-fork-delta.md`). Each fact is tagged original release or later fork.
2. **The meanings.** For every name another game or a lower layer already has, the reader settles
   what it means in each game (`porting/shooters-edge/cvar-meanings.md`): same thing, another
   default, another spelling, another meaning, or unsettled.
3. **The gaps table** (`porting/shooters-edge/<game>-client-gaps.md`, `<game>-server-gaps.md`): one
   row a name with what we do, its layer and its round.
4. **The rounds**, ranked: gameplay first (movement, prediction, mouse feel, weapon choice), then
   chat and scripting, then view, HUD, teamplay and network, cosmetics last.
5. **The name check** before each round: the names and their layers checked against every other
   game's, so no round lands a clash or a second name for one thing.
6. **Build the round**: each name in the lowest layer whose games all mean the same by it; record it
   in the row file (`docs/goals/<row>.md`) and the gaps table.

## The rules in brief

- Two spellings of ONE thing: one setting in OUR name (the terse, established style), each game's
  spelling a synonym in its own layer (`fov` for `cl_fov`; `ignore_tv` with `ignore_qtv` a synonym;
  the command `reset` with `cvar_reset`).
- Meanings that differ at ANY value are two settings (Quake II's `cl_noskins 2` and our
  `noskins 2`).
- A shared name that means something else in one game is `own` in that game. A synonym cannot
  shadow a live cvar: Quake II's `cl_maxfps` (its move rate) must be `own`, and its `r_maxfps`
  becomes a synonym of our frame cap.
- A per-game default is a `same` row: Quake II walks at 200, Quake at 400.
- A setting only one game can use lives in that game's layer even when shared code reads it
  (`cl_predict_crouch`, `cl_noskins`, `hand` in Quake II's).
- A feature one game had and another did not: on in its game, off in the other (`ledger.md`).

## The common names across the games

Once a game's own rounds are done, its names are checked against every other game's for the ONE
THING two games keep under two settings: `porting/shooters-edge/common-settings.md` is that audit
for Quake, QuakeWorld and Quake II (every name of the five reference sources, the 190 the games
share, each with our name, its layer and its status). A lift moves the setting to the lowest layer
its games share (bound to one field every game's code reads), makes each game's spelling a synonym
in its layer and its default a `same` row, and flips the audit's row.

## Gaps

- The common names left to lift, by batch: the graphs and the 2D switch (B4), the server's
  `cheats`, recording size and port (B5).
- `toggle` (flips in one game, cycles in the other): a per-game command layer.
- The move rate: Quake II's `cl_maxfps` as `own`.
- No census yet of the third game's settings.

## Lessons

- Adding user info grows the connect packet and moves the net labs' digests; prove it is the only
  cause by taking the line out and running the labs again.
- The precheck's digest gate can run out of memory (signal 9): run the labs it reached again with
  `xargs -P 3` before trusting the failures.
- A shared cvar takes its default from the game's settings, and its help words follow.
- A user info setting both games have (`rate`, `password`) lives in the classic layer with
  Quake's default; Quake II declares it `same` with its own. A change while joined is told the
  server at the next frame (Quake's `setinfo`, Quake II's whole user info again).
- A meta-cvar sets values under the explicit ones.
- Player text has no backticks and no `next:`.
- A ruleset locks a setting by the name its game's clients know (`r_drawparticles`): when ours is
  another name, the lock does nothing until that name is a synonym of ours (`Cvars::find` follows
  whole synonyms). Check every locked name resolves after a lift.
- A setting registered by only one game's layer and moved to a shared one must leave the game's
  "set aside" resets (`add_first`'s) and its `GameBindings` pointer with it, or loading the other
  game puts the player's value back to a default.
- A setting registered twice in ONE layer keeps the first registration's field: the second
  `bind` is ignored, not merged. To read a setting another file of the same layer owns, keep its
  `Cvar*` (`Q2Session::dist` reads `cam_dist`) instead of binding it again.
- Matching names across games by spelling finds only part of the pairs: Quake II's `con_height`
  is QuakeWorld's `scr_consize`. Pair by what each does, from both ledgers' descriptions.

## Wish

`ge server rules --game=<id>` and `ge cvar layers --game=<id>`: every rule and setting a game sees,
with its synonyms, layer and default, instead of grepping three tables (`docs/goals/ge-backlog.md`).
