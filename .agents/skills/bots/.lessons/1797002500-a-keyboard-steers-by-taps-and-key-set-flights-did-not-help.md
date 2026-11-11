---
title: A keyboard bot steers by tapping the next set in; flying the walk's ways as key sets did not cut a bump
date: 2026-11-11
scope: games/shooters_edge/bots/keyboard.cpp, plugins/shooter/look_ahead.cpp, plugins/shooter/pilot.cpp
concepts: [keys, keyboard, keysteer, key-set, look-ahead, foresee, bumps, tapping, side-key]
---

- Under the nearest key set, a wish up to 26 degrees off the view is pressed as plain W: the bot
  cannot bear gently round a corner and runs on into its wall. Walk bumps under keys rose at
  every angle of the view to the run, by 4x with the view ALONG it (0-22 degrees): the snap, not
  the view, was the cause. Steering between the two sets either side by tapping the further in
  for its share (a sigma-delta tally, `keysteer`) put e1m2's walk bumps 11.0 -> 9.0 a bot-minute
  and stuck 133 -> 70-85, and is how people press (their side key's runs: median 0.084 s, 4.3
  changes a second).
- Tried and dropped (six 3-minute seeds each): the look-ahead's ways flown as the key sets under
  the view (and the pursuit's set turned a set either way, the set against the velocity as a
  hard brake, the keys the keyboard holds flown for the hands' delay plus the keyboard's, a
  longer horizon): walk bumps 11.4 -> 11.0, every bump was foreseen but "every way bumps": the
  set's 45-degree step is the error, a flight cannot steer round it. With `keysteer` the pressed
  way averages the wish's, so analog flights fit it (9.1 against 9.0 keyed).
- Feathering a slow wish by the speed ALONG it fails when the body slides off it (a set off the
  velocity gains speed across it): along stays low, the keys stay down, and the bot runs past
  its run-up point into the wall at 360. Let go on the whole speed too (`kSlide`): run-up bumps
  2.0 -> 1.55 a bot-minute.
- Judge a key change on people's yardstick off the RECORDING (se-people `keys.*`), not the
  trace's keys: the calm view put along the walk raised W while walking 25 -> 32 % in the trace
  yet lowered W on steady runs off the recording 26.7 -> 23.7 % (people 40). The trace's
  `forward`/`side` split by mode (fights press W 10 %) says where to look.
- One seed's bump count swings by more than these changes (a single match diverges after
  seconds): judge on the six-seed set, read single traced seeds only for the causes.
