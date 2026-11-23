---
title: The reference manual reads the live console, so a name a game answers off the console is missing
date: 2026-11-23
scope: engine/console/manual.hpp, games/shooters_edge/client/manual.hpp, engine/server/main.cpp
concepts: [manual, game-words, add-game-words, reference-manual, describe, game-console, unknown-command, flags-table, pass]
---

`ge manual` lists only what is registered on a `Console` (cvars, synonyms, commands, command
synonyms) and the flags a program put in `Console::manual().flags`. Two traps found building it:
a window's `+` lines and `writemanual` run before the window has registered its later commands,
so the window writes its source from `-manual`, just before the first frame; and a dedicated
server's game with its own protocol answers `map`, `kick` and the rest through
`ServerGame::console` (the console's unknown hook), so they reach the manual only as declared
`ServerGame::words` (`GameWord`, registered by `add_game_words`): a word added to a game's console
path gets its row there. A new command belongs on the console with its help; a new flag needs its row in the program's flag table
(the parser refuses a flag with none).
