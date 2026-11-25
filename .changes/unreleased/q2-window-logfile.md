---
type: Changed
title: Quake II's logfile in the window means what it means on the server
---

In Quake II, the window's `logfile` 1 now writes a new log file and 2 or more adds to it; `logfile_flush` 1 writes each line at once and 2 holds nothing back; and `logfile_prefix` takes the same forms as `console_prefix`. A change to any of them closes the log ("Closing console log.") and opens it again ("Logging console to ..."), as on Quake II's dedicated server.
