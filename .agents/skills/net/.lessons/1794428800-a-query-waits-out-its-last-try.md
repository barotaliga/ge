---
title: A paged query is done only after its last try has had its interval to answer
date: 2026-11-11
scope: engine/net/master.cpp, tests/net_master_test.cpp
concepts: [master, query, paging, retry, timeout, done]
---

`MasterQuery` asks each page again every `ask_us`, up to `tries` times. The first version gave up
on a master the moment its last try went out, so the last page's answer, already on the wire,
was dropped: a 50-entry list read as 40. Keep a page "due" until it is answered (mark answered
pages with -1, unsent pages with INT64_MIN). Count a master as finished only once every page is
answered, or its tries are spent AND `ask_us` has passed since the last one.
`master_pages_a_long_list` holds this. The same applies to any ask-and-retry client the net
layer adds (the status poller, the hub's rounds).
