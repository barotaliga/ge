---
title: A match report's id is unique only per server, and only with a clock
date: 2026-11-13
scope: engine/server/server.cpp, engine/account/matches.cpp
concepts: [match-report, report-id, dedupe, trusted-servers]
---

The master takes a report once, keyed by the server's key and the report's id. The id mixes the
match's start in Unix seconds with the server's count of matches. A server opened with no
`unix_us` clock (a lab, a test) starts every match at second 0, so two such servers, or one server
recreated, give their first match the same id. A lab that sends reports from several stand-in
servers must give them distinct ids, or the master answers the second one "again" and never adds
it to the totals.

A bot never adds to an account's totals, even when a lab gives it an account: clear `bot` as well
when one stands in for a logged-in player.
