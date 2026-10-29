---
type: Fixed
title: ge ci reads a megabyte log line
---

`ge ci`, `ge build`, `ge test`, `ge lint` and `ge leaks` no longer overflow the stack on a very long log line (a dump, or a progress bar redrawn without a newline). Each line is cut to 1024 characters before a pattern scans it.
