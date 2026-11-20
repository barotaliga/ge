---
title: A precheck stopped part way can leave a lab's server on its port; look before reading a digest
date: 2026-11-20
scope: tools/precheck.sh
concepts: [precheck, server-launch, port, bind, stale-process, digest]
---

A precheck stopped by a time limit kept running its labs in the background, and one of them left
`gesample` listening on 27652. The next precheck's `server-launch` failed with "cannot listen on
0.0.0.0:27652 ... OS error 98" and a moved digest, which looked like the change's fault. Before
trusting a red digest after a stopped run, list what is still running (`pgrep -fa
"precheck|build/debug/labs|gesample"`), stop it, and run the lab alone: here it was green.
