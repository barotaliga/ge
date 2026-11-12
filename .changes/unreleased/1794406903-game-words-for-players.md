---
type: Changed
title: In-game messages read like a game
---

Every line a player reads in the game (the console's answers, the server's and the lobby's refusals, votes, saves, recordings, the bots' replies, menu hints) is now a plain sentence that says what happened and, when it helps, what to type instead (`Bots join only a coop game. Type "coopgame" to start one.`), with no `next:` hints or backtick marks. The log keeps its agent-actionable shape. A test holds every game's own directories, the console and the server to the rule, and the `se-console-words` lab shows the console's refusals over a real map.
