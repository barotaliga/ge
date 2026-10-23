---
type: Added
title: ge digest --diff prints only the labs that moved; ge golden diff pairs rows by key
---

- **`ge digest --diff`** runs every lab (or the one you name) at its smoke flags and prints only the labs whose digest is off `tests/labs_digests.txt`, or missing from it. It writes nothing. The summary names them, and the next step says how to move a digest you meant to move, or how to find what moved one you did not.
- **`ge golden diff`** pairs rows by their first word when a golden's row count changes and every row's first word is a unique key. A scenario inserted mid-table is then one row `added`, and a row taken out is one row `gone`. Before, every row after it showed as moved. The JSON gives each row's `change` and the diff's `by_key`.
