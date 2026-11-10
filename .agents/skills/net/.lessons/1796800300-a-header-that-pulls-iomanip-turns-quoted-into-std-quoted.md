---
title: A header that pulls in <iomanip> turns an unqualified quoted(s) into std::quoted
date: 2026-11-10
scope: games/shooters_edge/net/classic_admin.hpp, games/shooters_edge/net/classic_server.cpp
concepts: [adl, std-quoted, iomanip, forward-declaration, include-hygiene, classic-server]
---

`classic_server.cpp` has its own `quoted(const std::string&)` in an anonymous namespace. Once
`classic_admin.hpp` included `console/console.hpp` and the listen and remote headers, `<iomanip>`
came in with them and argument-dependent lookup on `std::string` found `std::quoted` as well: the
call became ambiguous, then a conversion error from `_Quoted_string`. A header the endpoint's
files include declares what it names (`class Console;`, `struct Listen;`, `struct Remote;`) and
leaves the heavy includes to its `.cpp`.
