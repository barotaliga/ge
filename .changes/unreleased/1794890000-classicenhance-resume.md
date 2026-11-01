---
type: Added
title: A cut-off classic enhance stage resumes where it stopped
---

`ge tool classicenhance -- run` keeps a journal for each stage but the pack and the models (`<stage>.partial` in the pack's cache folder), naming every item whose files are written. A stage cut off between items, by a cancel, a kill or a failure, skips those items on the next run with the same options and does only the rest. Changed options or `--force` start it over.
