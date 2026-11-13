# A map's fog and see-through liquids (M18i13, M18i14)

From a clean-room reader (the `clean-room` skill) over ezQuake (c633fae) and FTEQW (f09ac7c),
2026-11-13. Each fact is marked **read** (the source states it), **derived** (worked out from
what the source does) or **open**. Nothing was measured. d is the map's density as written, D the
density a map unit the renderer uses, z the distance in map units.

## The fog

**The keys** (read): a leading underscore is stripped from every worldspawn key.

- ezQuake: `fog`, exactly four numbers (density, red, green, blue), each clamped 0..1, else no fog.
  `skyfog` (or `r_skyfog`): the sky's share, clamped 0..1; absent, 0 under the map's fog.
- FTEQW: `fog` or `airfog` run the `fog` command with the value as its words: 1 number density
  (colour 0.3 grey); 2 density and a grey; 3 red green blue (density 0.05); 4 density red green
  blue; 5 then alpha (1); 6 then depth bias (0). Nothing clamped; colours above 2 warn. Colours
  go sRGB to linear in a linear renderer. `skyfog` forces `r_skyfog` for the map. `waterfog` and
  `skyroomfog` the same for the eye in a liquid and in a sky room.

**The curve** (read): D = d / 64; t = max(D x z - bias, 0); the surface keeps e^-(t^2) of its
colour, the rest the fog's, times alpha (FTEQW's fifth number). 50% fog at about 1066 units for d
0.05 (derived). z is the planar view depth per pixel (both). FTEQW also has `r_fog_exp2` 0 (e^-t)
and `r_fog_linear` 1.

**The sky** (read): not fogged by distance; mixed toward the fog's colour by a fixed share:
FTEQW `r_skyfog` (default 0.5) x alpha; ezQuake the map's `skyfog` (0 without it), or
`r_fx_fog_sky` (0.3) under the player's own fog.

**What is fogged** (read): the world, brush and alias models, fullbright texels (added before
the fog), opaque liquids: the colour mix. Blended sprites and particles: their alpha times the
surface's share. Additive ones: their colour times it (to black). See-through liquids: ezQuake
mixes then applies the liquid's alpha; FTEQW fades the alpha.

**The command and the reset** (read): FTEQW's `fog <density> <r> <g> <b> <alpha> <bias>`, with
no words printing "Current fog D (r:R g:G b:B, a:A bias:B)". A server's `fog` (stuffed text in
the QuakeWorld protocol; message 41 or 51 in others) locks it against the player's while its
density is not 0. A typed fog fades in over 1 s. Every fog state resets at a map change (both).
ezQuake has no command: `r_fx_fog*` cvars, the map's fog used only in a local single-player game
by default (`r_fx_fog_usemap` 2).

## See-through liquids

**The settings** (read):

- ezQuake: one `gl_turbalpha` (1) for every `*` texture; the worldspawn's `wateralpha` and kin
  are parsed and unused (derived).
- FTEQW: `r_wateralpha` (1); `r_lavaalpha`, `r_slimealpha`, `r_telealpha` (empty: water's). The
  worldspawn's `wateralpha`, `lavaalpha`, `slimealpha`, `telealpha` force them for the map. A
  value at or under 0 is not drawn; 1 or more opaque. Kinds by texture prefix: `*lava`,
  `*slime`, `*tele`, any other `*` water.

**The permission** (read): neither checks the map for vis through liquids; `r_novis 1` is the
escape on a map not built for it (or FTEQW's `.vis` patch beside the map).

- ezQuake: the serverinfo `watervis` (0..1) is the allowance: opacity clamped to 1 - allowance
  .. 1; missing is 0 (opaque). Spectators and demos are free under `r_novis` or any allowance.
- FTEQW: allowed for spectators, demos, a non-zero `watervis`, an absent `watervis` with
  `ruleset_allow_watervis` 1 (the default), or bit 1 of `allow`; else every liquid opaque.

**The drawing** (read): after the opaque world, blended, depth tested and not written, one-sided;
ezQuake also merges the visibility of the leaf across the surface (10 units above in a liquid, a
view height below in the air).
