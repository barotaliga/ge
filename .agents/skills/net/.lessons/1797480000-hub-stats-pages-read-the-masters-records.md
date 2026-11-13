---
title: The hub's stats pages read the master's records in its own process, not over the wire
date: 2026-11-13
scope: engine/hub/stats_page.cpp
concepts: [hub, player-page, leaders, match-report, totals-filter, phone-width]
---

The Player, Match and Leaders pages need the account service's `Matches` (totals and the kept
reports), which only a hub run with `--accounts` holds, so `stats_answer` is routed in `http.cpp`
beside `account_answer`, before `hub_answer`, and `HubSpec::stats` (set by the hub's main) is what
puts Leaders in the nav bar. Totals carry no dates: "the last N days" is summed from the kept
reports (`totals_by_account` with `since_s`), so it reaches back only as far as the last 256.

A game's score is often its frags (`HubSpec::score == "frags"`): a scoreboard with both columns
shows the same number twice. And a wide stats table on a phone: `display:block; overflow-x:auto`
on the table itself scrolls it inside the page, where a wrapper is easy to forget on one of many.
