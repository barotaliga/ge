---
title: A server table the rules must read goes in a header, its rule a template on the caller's draw
date: 2026-11-27
scope: games/shooters_edge/server/q2_monster_pain_rules.hpp, games/shooters_edge/server/q2_monster_pain.cpp, games/shooters_edge/rules/campaign_q2.hpp, games/shooters_edge/rules/campaign_q2_kinds.cpp
concepts: [remix, pain, pains-as-data, campaign, link-order, strings, shared-mind]
---

`shooters_edge_rules` is a library the server links, never the other way, so a rules file cannot
call a function defined in a server .cpp. To share data like Quake II's pain table, move the
table and its rule into a header-only file (constexpr rows, a `template <class Roll>` rule that
calls `roll()` where the server called its PRNG). The server's .cpp keeps its old signature as a
thin wrapper, so its draws and bits are unchanged. A cry the campaign plays must be in its string
table at build time: a constexpr reader over the table (`q2_pain::cry`) lists each kind's cries,
and a `static_assert` checks each kind fits the slots. The campaign has no "damage taken"
argument in a monster's hurt call, so it keeps the health after the last blow in a field and
takes the difference (reset when the health rose, as after a medic's raise).
