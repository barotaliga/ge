---
type: Added
title: a bot's keys pressed by a hand on a keyboard, and the keys read off recordings
---

`ge::ai::Keyboard` (`engine/ai/keys.hpp`) turns the actions a bot wants into the keys a hand on a keyboard can press: five fingers on a home placement, one key a finger, a reach, a finger's time to press, let go and travel, taps, and the whole hand moving off its place to a far key. Shooter's Edge's bots can press their movement and weapon keys through it (the `keys` feature, off in every preset until it is judged), and steer between two key sets by tapping the further one in for its share of the time, as a person bears right with W held and D tapped (`keysteer`, with it). The `se-people` lab reads the movement keys off people's and bots' recordings alike (`people.keys.*`), and the new `se-keymap` lab draws a bot's path from above with each step coloured by the keys it pressed.
