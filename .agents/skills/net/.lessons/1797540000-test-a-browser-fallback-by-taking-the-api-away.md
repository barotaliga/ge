---
title: Test a page's fallback by taking the browser's API away before the page loads
date: 2026-11-14
scope: games/shooters_edge/web/join.cpp
concepts: [webtransport, websocket-fallback, chromium, playwright, add-init-script, play-in-browser]
---

Headless Chromium has WebTransport, so a drive never reaches the WebSocket path on its own. Take
the API away before any of the page's script runs (Playwright's `context.addInitScript(() => {
delete window.WebTransport; })`) and the player's `wt_open` returns -1 exactly as in a browser
without it, so the fallback runs on the real hub page with the real forwarder. Check it took with
`typeof window.WebTransport` after the load, and that the forwarder's log says `websocket: page ...`.
The same trick covers any other browser API a page falls back from.
