# Player skins (M18i15)

From a clean-room reader's answers (two maintained multiplayer clients of the first game; no code
was read by the implementer). Each fact is marked **read** (the reader's account of the source's
behaviour) or **derived**.

## The name

- A player's `skin` userinfo names `skins/<name>.pcx`; the extension is dropped first, so `foo.pcx`
  and `foo` are one skin (read). A `/` in the name is kept (read).
- An empty name falls back to `baseskin`; one client defaults it to `base`, the other to empty
  (read). We default to `base`.
- A name holding `..` anywhere or starting with `.` becomes `baseskin` (read).
- One client keeps 15 characters of the name, the other 63 (read). We cut at 15.
- `allskins <name>` forces one skin on every player; with no argument one client prints the
  value, the other clears it (read). We clear it.

## The picture

- PCX version 5, run-length, 8 bits a pixel only; the file's own palette is ignored, its pixels
  are indices into the game palette (read).
- Over 320 wide or 200 tall is a failed load and falls back (read).
- A smaller picture sits at the top left of a 320x200 field filled with index 0; the skin drawn is
  that field's top-left 296x194, stretched over the model's whole texture space whatever the
  model's own skin size (read, one client; the other samples differently).
- A missing file falls back to `baseskin`, then to the model's own skin, still recoloured (read).
- Indices 224 and up glow (read, one client builds a glow layer from them).

## The colours

- The shirt range is indices 16-31 (row 1), the pants 96-111 (row 6) (read).
- A colour c is clamped to 0-13 (one client); for c below 8 skin index 16 + i becomes 16c + i, for
  8 and up it becomes 16c + 15 - i: the row runs backwards (read, both).
- Derived: rows 8 and up are stored bright to dark, so the backward read keeps the painter's
  shading; the tint is the bright end of the row in either direction.

## Settings

- `noskins` (default 0): 0 skins used and fetched, 1 every player in the model's own skin, still
  recoloured, 2 skins on disk used but none fetched (read).
- `baseskin`: the fallback name (read).
- `skin`: the player's own userinfo key, default empty (read).

## 24-bit skins

- One client tries `skins/<name>.tga`, `.png`, `.jpg` before the .pcx; a 24-bit skin is drawn as
  painted, with no shirt or pants remap and no glow layer (read). We read .tga and .png in that
  order (no JPEG decoder in the engine).
- The other client also looks for `<name>_shirt` and `<name>_pants` overlays tinted by the colours
  (read). Not built.

## Which models

- Only the player model (`progs/player.mdl`) wears a player's skin (read, both). Corpses with a
  colormap wear their player's skin in both clients (derived); not built yet (our corpses carry no
  slot).
