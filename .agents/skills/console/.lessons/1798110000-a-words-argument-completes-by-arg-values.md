---
title: A word's argument completes by Console::set_arg_values, even a word the console forwards
date: 2026-11-23
scope: engine/console/console.hpp, engine/console/tab_list.cpp, engine/console/view.cpp, games/shooters_edge/client/q2_bar.cpp
concepts: [arg-values, argument-completion, tab-completion, forwarded-words, item-completion]
---

Completing what follows a command word (an item name after `use`) is `Console::set_arg_values(word,
fn)`: the function is asked when the line is completed, so it returns what is live now (the last
frame's item names), and the word need not be a registered command (Quake II's `use` reaches the
rules through `set_unknown`). Both Tabs read it: the listing Tab (`tab_list.cpp`'s `arg_tab`, the
shared part found on the bare names, a name with a space quoted in the line) and ours (the
suggestions, `value_typed`). A cvar's own values still come first. Wrapping a built-in for one game
(Quake II's `help` alone forwarded) copies `commands().at(name)` and re-adds it with the old `fn`
called for the rest.
