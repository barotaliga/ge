---
type: Changed
title: parallel_bands runs on a persistent worker pool
---

`ge::core::parallel_bands` no longer starts a thread a band on every call: its bands run on the calling thread and a pool of worker threads started on the first call and kept for the process, each thread taking the next band left. A call costs a wake-up instead of thread starts, so a small grid baked on eight workers is no longer slower than on one, and back-to-back bakes find the workers awake. The bands, and so every result, are the same bits as before; a band no longer runs on a fixed thread (band 0 was the caller's), and a call made from inside a band, or from a second thread while one bake is running, runs its bands in order on its own thread. The `parallel-cost` lab times a call by grid size and worker count.
