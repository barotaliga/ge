---
type: Fixed
title: A server started plainly keeps serving, and a client leaves a server that went quiet
---

`sesv` (and every game's dedicated server) started without `--for`, `--offline` or `--replay` now serves on the wall clock until it is stopped. Before, it ran 60 seconds of server time as fast as it could and quit within about two seconds. A connected client, classic or sequel, now gives up a server that has sent nothing for 60 seconds and says "the server stopped answering", where before it waited forever.
