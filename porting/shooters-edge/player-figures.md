# Player figures in the second game (M18i16)

From a clean-room reader's answers (the second game's original release and two maintained clients;
no code was read by the implementer). Each fact is marked **read** or **derived**. The first game's
skins are [player-skins.md](player-skins.md).

## The look

- There is no `model` setting: the player's `skin` (default `male/grunt`) is "<model>/<skin>"; the
  server broadcasts it unchanged beside the name (read).
- The folder is the text up to the first `/`, else up to the first backslash; with neither the
  folder is empty and the skin is the text minus its first character (read, derived).
- Order (read): `noskins` (or no look) gives male in grunt; a folder whose `tris.md2` is missing
  becomes male; a skin missing from the folder (when the folder is not male) switches the figure to
  male and tries male's skin of that name (so team skins shipped for male work on any figure); then
  the folder's `grunt`. Built in `choose_look`.
- All-or-nothing (read): a missing icon `<skin>_i.pcx` or `weapon.md2` throws the whole entry back
  to male/grunt. Not built: we draw what is there.
- `gender` is set from the folder when `gender_auto` is on: male and cyborg male, female and crakhor
  female, else none (read). Not built.

## The weapons a figure carries

- The game names its carried weapons with a leading `#` (`#w_shotgun.md2` and so on, eleven in the
  weapons' order); the client loads each as `players/<folder>/<name>`, entry 0 being `weapon.md2`
  (read). A missing one falls to the folder's `weapon.md2`, then male's (read). The weapon is drawn
  in its own first skin (read). Built: each folder's carried weapons, drawn in place of the server's.

## 24-bit pictures

- The original reads PCX only; a maintained client tries `.tga`, `.png`, `.jpg` before a `.pcx`,
  taking a replacement only when it is at least as large as the .pcx (read). We read .tga, .png,
  .pcx in that order and take the first that reads (no size rule).

## Downloads (not built yet)

- Only while connecting, before `begin`: per player, `players/<m>/tris.md2`, `weapon.md2`,
  `weapon.pcx`, `<skin>.pcx`, `<skin>_i.pcx`, each skipped when present; never the `w_*.md2` (read).
- Settings `allow_download` and `allow_download_players` (default 0) on both sides (read).
- Files under `players/` are written under the base game's folder, through a `.tmp` name (read).
