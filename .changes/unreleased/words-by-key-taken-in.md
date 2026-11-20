---
type: Fixed
title: A taken-in release's messages show their words, not their keys
---
Quake's later release prints its messages by key (`$qc_backpack_got`) and keeps the words in its
own archive beside the game folder. Taking the game in copied the packs but not that archive, so
single player showed the keys. The words file is now taken out of the archive and kept with the
game.
