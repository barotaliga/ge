# Working notes: the porting behind each milestone

A WORKING DOCUMENT (`porting/` is deleted when porting ends). `docs/goals.md` states each
milestone in the engine's own terms; this page keeps what the row meant against the
reference.

| # | Against the reference |
| --- | --- |
| M0 | `core/prng` and `core/math` (hypot) at bit parity with oss-game-framework. |
| M1 | `core/libm` matches the oracle's runtime: game5's pinned Node 24 (V8 13.6's fdlibm, and glibc's pow on Linux x86-64), measured in `research.md` §2. `core/num` is the JS semantics helpers (`README.md` § JavaScript semantics that bite). |
| M2 | Every function game5 calls has a golden test from the framework. Skipped because game5 calls none of them: `polyline`, `tiledValueNoise`, `TWILIGHT`, `decay`, `blockOffsets`. |
| M3 | A game5 ghost tape decodes and re-encodes byte for byte. `readBook` and `isControlTape` (the stored-JSON readers) move to M10 with the ghost and record stores. |
| M4a | `levelDigest`, every grid and every section of the level match game5's `generateLevel` at version 1 (`singlePiste`) on `LEVEL_SEEDS`, the benchmark's map, a map in each crusted region and two seeds whose first attempts are refused. About 1.5× faster than the oracle: 0.53–0.77 s a map against 0.76–1.0 s under Node 24. |
| M4b | `levelDigest` matches game5 on every pinned race map and trick map (`race-maps.ts`, `trick-maps.ts`, all v8); `analyzeResort`. |
| M4c | `levelDigest` matches game5 on every jump hill (`jump-hills.ts`, v9) and on a v10 seed sweep. |
| M4d | `levelDigest` matches game5 on every real face's map. |
| M5 | `createGame` and `step`; the `make sim` digests match game5's for the reference seeds. |
| M7–M8 | Screenshots against game5's `make world` views (`subsystems.md` § render). |
| M9 | The framework's synth's design (M9a); game5's bank and beds (M9b). No render parity: the meters decide. |
| M13+ | game5's free-ride systems, one row each as game5's `docs/features.md` lists them. |
