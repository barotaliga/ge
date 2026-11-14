---
type: Added
title: Save slots and the store
---

`save::Store` keeps a game's saves in one directory: the autosave ring with its spacing, the quicksave and numbered slots, an index that lists a thousand slots with one read and is rebuilt from the slots' headers when it is missing or damaged, and writes that go through a temporary file, a read-back and a rename so that a crash at any point leaves the last good save loadable. Delete, copy and rename are store calls. `save::StoreWriter` does the disk on a thread of its own and answers each save in order. `save::DirDisk` is a real directory and `save::SimDisk` one in memory that can be killed after any byte. The `save-slots` lab.
