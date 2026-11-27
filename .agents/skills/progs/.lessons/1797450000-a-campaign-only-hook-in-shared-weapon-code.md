---
title: Mark a shot's landing for the campaign inside the shared weapons, behind its build flag
date: 2026-11-27
scope: games/shooters_edge/rules/common_weapons.cpp, games/shooters_edge/rules/common_q2_arms.cpp, games/shooters_edge/rules/campaign_q2_kinds.cpp
concepts: [remix, hearing, impact, campaign, arena, build-flag]
---

The weapons are compiled into both the arena and the campaign. A call only the campaign has
(`sq_note_impact`, `sq_offer_dodge`) goes inside `#if defined(SE_RULES_CAMPAIGN)` at each site,
and the campaign's own glue (`campaign_glue.cpp`, Quake's bullet chip) may call it bare. The
arena's bits stay the same, and the campaign's digests stay the same too while only Quake II's
monsters read the mark. A campaign test can set a player's field by name
(`pg::find_field(mod(), ...)`) to stage a noise no weapon made.
