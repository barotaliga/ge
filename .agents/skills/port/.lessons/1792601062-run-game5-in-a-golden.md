---
title: A golden script can run game5's own code with no npm install, through a resolve hook
date: 2026-10-21
scope: porting/parity/golden-racing.mjs
concepts: [golden, oracle, resolve-hook, ghost]
---

A golden script can run game5's own code (its engine, `pwa/src/game/*.ts`, `tests/support/*`)
with no `npm install` in the oracle: a `registerHooks` resolve hook maps
`@niclaslindstedt/oss-game-framework/<sub>` to `../oss-game-framework/src/<sub>.ts` and
`@engine` to `../game5/engine/index.ts` (`porting/parity/golden-racing.mjs`). A bot lap of the
synthetic slope then takes about a second, so a real game5 artifact (a ghost tape) can be the
fixture instead of a hand-made one. Check the framework checkout is at the tag game5's
`package.json` pins before trusting it.
