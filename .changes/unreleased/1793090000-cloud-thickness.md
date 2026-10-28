---
type: Added
---
Clouds have a thickness that varies across them, and it decides how much of the sun and the moon shines through: a sheet's thin patches show the disc (bright, as through frosted glass), a thicker part melts it into a bright smudge and its thick cores hide it, and the cloud's shadow on the ground follows. Each kind has its own range (altostratus from a watery sun to hidden, stratus a watery disc in its thinnest patches, a rain deck never), each map leans its sky thinner or thicker, and Fall Line's `sv_thick` (0 thin .. 1 thick) picks it by hand. Fixed: the sun no longer shows through a thick deck as a disc, and a cloud in front of the moon no longer shows as a dark spot. Whole-line comments in a shader no longer count toward its embedded size.
