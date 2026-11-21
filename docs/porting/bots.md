# Bots: what every game's bots share, and what each game's own

Bots that play a classic game like people: they see, hear, remember, choose goals, move with the
game's own tricks, aim with human hands, talk to their team, and join co-op as companions. The
split is the maintainer's (`docs/porting-games.md` § Bots); this page places the files.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/ai/` | The decision record `ge why` reads, the keyboard and mouse hands, thinking on a schedule, sight, memory, paths, steering, choosing (`bots` skill) |
| Shooter kit | `plugins/shooterbots/bot.hpp`, `cmd_source.hpp` | The bot as a server slot's command source |
| Shooter kit | `plugins/shooterbots/nav.hpp`, `nav_gaps.hpp`, `nav_script.hpp`, `swim.hpp` | Navigation proven by the move: floor nodes off the hulls, links proven by `player_move`, gap tricks, scripted ways, the way out of water |
| Shooter bots | `plugins/shooterbots/pilot*.hpp`, `look_ahead.hpp`, `recipes.hpp`, `hops.hpp`, `rocket_jumps.hpp`, `gap_jumps.hpp`, `jump_finder.hpp` | The pilot playing a route command by command; movement read off people's recordings |
| Shooter kit | `plugins/shooterbots/senses.hpp`, `hearing.hpp`, `aim.hpp`, `aim_path.hpp`, `intercept.hpp`, `dodge.hpp`, `escape.hpp`, `brink.hpp`, `idle.hpp` | Senses, ears, aim through skill hands, leading a shot, dodging, escaping a pit, the brink, idling |
| Shooter kit | `plugins/shooterbots/traits.hpp`, `temper.hpp`, `features.hpp` | Personality, temperament, behaviours switched by name |
| Shooter kit | `plugins/shooterbots/team_talk.hpp`, `bot_view.hpp`, `intent_draft.hpp`, `view_book.hpp` | What a team message MEANS; what a bot sees of a match; a map's intent drafted; people's view yaw by spot |
| Classic plugin | `plugins/classic/item_tables.hpp`, `item_roles.hpp`, `locs.hpp` | The items' shapes and roles, place names |
| Shell | `games/shooters_edge/bots/` | The fighter (mode, goal, weapon, trigger), knowledge, ears, packs, mind, team, map plan, footwork, water, companion, coop and unit runs, handles |
| Shell | `games/shooters_edge/net/qw_bot_team.hpp`, `qw_team*.hpp`, `qw_bot_feed.hpp` | A team of bots on any classic server over UDP clients; the team's shared map and beliefs; a live feed |

## Each game's

| | Quake | Quake II | Next |
| --- | --- | --- | --- |
| Movement tricks | the hop, gap and blast-jump tricks, footwork, swimming, read off its people's recordings (`bots/hop_start_book.hpp`, `rocket_tricks.hpp`, `gap_tricks.hpp`) | its own air law; the fighter on its rules (`bots/q2_unit.hpp`) | its own tricks, from its own recordings |
| Item worths, weapon choice | `bots/fighter_parts.hpp`, `bot_arsenal.hpp` | its rows in the same shapes | its rows |
| Team talk's words | `bots/talk.hpp`: its say lines, macros, loc file | named off the map's items | its own wording |
| Map plans, routes | `bots/map_plan.hpp`, `item_routes.hpp` | its maps' | its maps' |

## Adding a game

1. The nav built from the game's maps through the shared trace (a 38 map's nav is still open).
2. Its items as roles and worths; its weapons as the bots see them (`bot_arsenal.hpp`).
3. Its movement: how it drives the shared hands, tuned to its own numbers. A hop or a blast jump is
   the game's own, never one trick with a knob per game.
4. Its team talk's words over the shared meanings (`say_text`, `parse_say` over `SayKind`).
5. Its co-op companion on its rules; its unit played through by a bot with a golden digest.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Bots that learn moves from people's recordings | ours, on Quake's community demos | `plugins/shooterbots` (recipes, hops, rocket and gap jumps) |
| Team talk understood both ways, bots and people alike | Quake's teamplay culture | `team_talk.hpp`, each game's words |
| Human hands: reaction, aim spread, keyboard taps | ours | `engine/ai` |
| Co-op companions | ours | `bots/companion.hpp`, both campaigns |

| Drawback left behind | How |
| --- | --- |
| Bots that cheat by reading the world | senses: what a client is sent, sight by trace, sounds heard by skill |
| Waypoint files hand-made per map | nav proven by the move from the map itself |

## Gaps

The nav and the bots' world on a 38 map; bots in Quake II's deathmatch on its maps.

## Labs and tests

`se-bots`, `se-nav`, `se-pilot`, `se-team`, `se-duel`, `se-q2-bots`, `se-why`, `se-trick`,
`se-recipes`, `se-senses`, `se-talk`; `tests/shooters_edge_*bot*_test.cpp`.

## Lessons

- Bot movement is each game's; share the decision architecture (goals as scored options, claims,
  giving up), memory and the model of other players.
- Move a bot part up only when the next game shares it unchanged.
