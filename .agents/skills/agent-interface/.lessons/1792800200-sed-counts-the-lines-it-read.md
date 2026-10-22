---
title: An edited input log's line numbers are the original file's
date: 2026-10-22
scope: .github/workflows/packaging.yml
concepts: [input-log, diverge, sed, ci, scaffold]
---

`sed -e '/^digest/d' -e '/^steps/d' -e '9s/.*/.../'` numbers lines as it reads them, before the
deletes: line 9 is the original log's second run line, not the ninth line written. A wrong
number edits a header line instead, the logs never part, and `ge diverge` answers "the same run".
Check the edit with `ge diverge` locally before it goes into a workflow.
