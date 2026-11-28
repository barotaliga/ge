# Shooter's Edge content

Shooter's Edge has no game of its own: it is the menu and the moon backdrop, and it loads the classic
games the player owns (Quake, QuakeWorld, Quake II) and remixes them. What is here exists only for the
BUILT-IN ARENA, the free match the headless tests and labs run because the classic packs are never
committed; no player starts it, and a classic game always plays and shows its own content. Read
`.agents/skills/free-assets/SKILL.md` before changing anything here.

| File | What |
| --- | --- |
| `sounds.tsv`, `models.tsv` | The registry: every sound the arena plays and the held weapons its labs draw, by our name, with what plays or shows it |
| `classic.assetmap` | The classic sound names (and the numbered sound sets maps pick) mapped to ours, so the arena's mod is heard |
| `sounds/`, `models/view/` | The files: mono 16-bit 22 050 Hz WAV; the held weapons built in Blender (`ge asset build`) |
| `maps/` | The arena's plans (`secli map build`), the labs' maps |
| `CREDITS.md` | Where each file came from, who made it and its licence |
