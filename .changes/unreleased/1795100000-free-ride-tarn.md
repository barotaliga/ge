---
type: Added
title: A tarn on Fall Line's free ride
---

The mountain plugin can lay a tarn in a cirque on a generated map (`mountain/tarn/tarn.hpp`,
`Level::lakes`), its ice by the winter so far: frozen flat where it bears a skier, open water or
thin black ice over a dug bowl where it does not. Fall Line's free ride lays one, draws it as water
or ice (`app/lakes.hpp`, the `tarn` and `tarn-high` views, the `tarn` lab), and a skier through
thin ice sinks (a `sank` event) and is stood back on his run.
