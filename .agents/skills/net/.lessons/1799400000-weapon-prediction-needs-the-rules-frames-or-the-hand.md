---
title: Weapon prediction shows shots, not frames, when the rules never animate the weapon
date: 2026-11-23
scope: games/shooters_edge/net/qw_weapon.cpp
concepts: [weapon-prediction, predicted-sound, own-shots, held-weapon, weaponframe, arena-rules, double-shot]
---

Our arena's rules in C++ refire by the clock alone and leave `weaponframe` at 0, so predicted
frames (1, 2 after each shot) would flicker against every server frame's 0. Draw the predicted
frame only once the server has sent one other than 0, and otherwise count each predicted shot in
`DemoState::own_shots` for the hand to fire its own clip: that is also what made the held weapon
fire at all on a joined server of ours. Play a shot once: commands up to `played_to` never replay,
and a shot a correction moves by a command (the server's clock is its frame's, not the commands')
is the same shot when the same sound's weapon clock is within 0.05 s. Drop the server's copies of
the sounds BEFORE adding the predicted ones: both sit on the player's entity on channel 1.
Measure with `se-weapon-predict` (press to the first shot heard, shots a press, off and on).
