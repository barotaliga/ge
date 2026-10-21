---
title: A roadmap row is a guess at what game5 needs; the import survey is the answer
date: 2026-10-21
scope: engine/core, plugins/racing, porting/parity/oracle-import.mjs
concepts: [survey, framework, scope, oracle]
---

The milestone list named `polyline`, but game5 imports nothing from
`core/polyline` (it keeps its own segment distances in the modules that use them),
and calls neither `tiledValueNoise` nor `TWILIGHT`. Before porting a framework module, list
every symbol game5 imports from it across `engine/`, `pwa/src/` and `scripts/`
(including what `engine/index.ts` re-exports for the app and labs, e.g. `fillField`,
`moonAt`), port those plus their internal helpers, and record what was skipped in the
port's header and in the roadmap row. Keep the port's `ge:keywords` to what was ported, so a
search for a skipped symbol finds nothing rather than a file that lacks it.

Golden generators cannot `import()` the framework's `.ts` sources bare: their relative
imports have no extension. `porting/parity/oracle-import.mjs` adds the retry hook.
