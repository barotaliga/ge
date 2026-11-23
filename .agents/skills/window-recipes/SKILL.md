---
name: window-recipes
description: "Use to drive a game's WINDOW with no one at the keyboard in gameengine2: a recipe (a cfg run with +exec) that joins a server, opens menu pages, presses keys, reads the page back as text and takes screenshots, then quits. Owns the recipe commands (the console's `sleep`, Shooter's Edge's `menu_press`, `menu_select`, `menu_list`, with `screenshot` and `condump`), the runner scripts/run.sh, the recipes kept here (SERVER SETTINGS read and changed), and checking a menu against live or local servers (a local QuakeWorld or Quake II server, files that match the server's). Load it before asking a person to click through a menu or to look at the screen."
---

# Window recipes

A RECIPE is a cfg that drives the window as a player would: the console language and nothing
more, run with `+exec`, so it needs no build of its own and works for any game the window loads.
`docs/console.md` § Recipes is the reference; this skill is the procedure. Ask a person to click
or look only when a recipe cannot answer (sound, feel, a GPU fault the picture does not show).

## The commands

| Command | What it does |
| --- | --- |
| `sleep <seconds>` | The rest waits that long in wall time (the window's clock). Use it, not chains of `wait` |
| `menu_options`, `menu_main`, `pushmenu <page>` | A page opened directly |
| `menu_select <item>` | The item with that id focused and pressed (ids from `menu_list`) |
| `menu_press <key>...` | Keys, one a frame: `up down left right enter escape del refresh` or a letter |
| `menu_list` | The page up as text: `>` focus, `-` disabled, `id "WORDS" = value -> page` |
| `screenshot <name>` | The next frame as a picture in the game's `screenshots/` |
| `condump <name>` | The console's lines to a file: the recipe's whole record |
| `connect`, `disconnect`, `quit` | As a player types them |

Arguments are cvars set before the recipe (`+set recipe_server host:port`), read as `$name`.

## The loop

1. Build the window: `cmake --build --preset release --target se`.
2. Pick or write a recipe (`recipes/` here). Read pages by id with `menu_list` first, then
   `menu_select` by id: ids are stable, counted `down` presses are not.
3. Run it: `.agents/skills/window-recipes/scripts/run.sh <recipe.cfg> --game=q1|q2
   --basedir=<dir> recipe_server=<host:port> --out=<name>`. It prints the console's dump (each
   `menu_list` page in it) and the screenshots' paths.
4. Judge from the text first, then the picture. Put both on the report's page.

## Rules that cost a round each

- **Give the window time.** The first `sleep 3` lets the window open before `connect`; a join takes
  10 s or more on a stock server (the map, the model checks, a match mod's greeting).
- **Match the server's files.** A stock QuakeWorld server checks the map's checksum and drops a
  client whose map differs, saying "Map model file does not match". A rerelease install's maps
  are not the originals: join with `-basedir` set to the server's own files.
- **A change on a public server is felt by its players.** `server-settings-change.cfg` is for a
  server of your own only; on a public one run the read-only `server-settings.cfg`.
- **Local servers first.** A local stock server (a QuakeWorld server with its match mod, a Quake II
  server) answers the same as a live one and can be changed freely. Start it only with the
  person's go-ahead when they have asked not to run it.
- **Escape is not always the menu.** In a level, `escape` may put a screen away first (Quake II's
  help computer). `menu_options` opens OPTIONS in every game.
- **The window needs a display.** On the Linux container use a lab instead; a recipe is for a
  desktop (the Mac).

## Recipes kept here

| Recipe | What it answers |
| --- | --- |
| `recipes/server-settings.cfg` | OPTIONS ▸ SERVER SETTINGS on a joined server, read and pictured, nothing changed |
| `recipes/server-settings-change.cfg` | The same, the second row stepped once and the page read back (own servers only) |
