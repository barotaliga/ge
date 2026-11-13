# Voice and chat targets in the classic protocol's voice extension

Facts from a clean-room reader (`.agents/skills/clean-room/SKILL.md`) of the three projects
`survey.md` names for the client, the server and the widest extension set, at their main branches
of November 2026. Every fact here is `read`; none was measured. No code was copied.

## Who hears a voice

- A client names its voice's target with the string command `voicetarg <word>`, sent once on the
  reliable channel and kept by the server for the connection. Each voice message is routed by the
  target held when it arrives. Words: `team` (the talker's team: the same team string, no
  spectators; a spectator reaches spectators; everyone when team play is off), `all`, `nonmuted`
  (all but those the talker ignores), `specself` (one project only), or a decimal slot from 0 (one
  player). A word the server does not know sets `team`, by design, so newer words never break
  older servers. A slot past the last is ignored.
- Every connection starts with target `team` and with voice reception OFF: a listener hears
  nothing until its client sends `unmuteall`; `muteall` stops it. `vignore <slot> [0|1]` mutes one
  talker for that listener (no number toggles).
- `sv_spectalk 0`: a spectator's voice reaches only spectators.
- The client settings are `+voip`, `cl_voip_send` (0 push to talk, 1 voice activation, 2 always),
  `cl_voip_play`; no client setting or button picks the target. A client sends `unmuteall` or
  `muteall` on every map change by `cl_voip_play`.
- Server settings: `sv_voip` (1), `sv_voip_echo` (0), `sv_voip_record` (0), `sv_spectalk` (1).
- None of the three routes or attenuates voice by distance, and none has a chat to the players
  near (no proximity say). Ours (`voicetarg near`, `say_near`) is new; on a classic server
  `near` falls back to `team`.
- Nothing is sent back to a client about its target on two of the three; one prints a
  confirmation when asked.

## The chat line

- `messagemode` sends `say`, `messagemode2` sends `say_team`; no `messagemode3` exists in any of
  the three. The prompts read `say:` and `say_team:`. The default keys differ by client (t and y
  in either order).
