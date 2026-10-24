---
type: Added
title: The console, cvars and cfg files
---

`engine/console` (`ge::console`, published as `ge::console`): every setting is a cvar with a short prefixed name (`sv_`, `cl_`, `r_`, `s_`, `vid_`, `net_`, `con_`) and a tier (a player's, archived by `cfg_save`; the server's, sent in the server info; cheat; latched; read-only), owning its value or bound to a field of a settings struct. Cfg files run the console language: `exec`, `alias` with `$1`..`$9` and `$*`, `$cvar` expansion and macros, `wait`, `if`/`else`, `set_calc`, `inc`, `dec`, `toggle`, `cycle`, message triggers, `bind` by mode with modifiers, `cfg_save`, and the startup order `default.cfg`, `config.cfg`, `autoexec.cfg`, then `+commands`. Every problem names its file and line and the nearest known name. The drop-down console's model (scrollback by kind, the input line, history, completion, `/` commands and plain-text chat, `say`, `say_team`, `whisper`, the tilde key) is pictured by the `console` lab. A game registers its cvars on the play contract (`GameSpec::cvars`), and `ge cvar ls|get` and `ge cfg check|exec|dump` answer for it headless.
