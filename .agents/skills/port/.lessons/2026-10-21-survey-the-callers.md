---
skill: port
scope: framework modules
---
The milestone list named `polyline`, but game5 imports nothing from
`core/polyline` (it keeps its own segment distances in the modules that use them),
and calls neither `tiledValueNoise` nor `TWILIGHT`. A roadmap row is a guess at what
game5 needs; the import survey is the answer. Before porting a framework module, list
every symbol game5 imports from it across `engine/`, `pwa/src/` and `scripts/`
(including what `engine/index.ts` re-exports for the app and labs, e.g. `fillField`,
`moonAt`), port those plus their internal helpers, and record what was skipped in the
port's header and in the roadmap row.

Golden generators cannot `import()` the framework's `.ts` sources bare: their relative
imports have no extension. `tools/parity/oracle-import.mjs` adds the retry hook.
