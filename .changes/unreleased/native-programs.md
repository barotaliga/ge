---
type: Added
title: Rules written in C++ run where a mod's bytecode runs
---

A program's functions can now be C++: `plugins/progs/native.hpp` builds a progs from a declaration (the program form, its own globals, fields and strings, its functions by name), and the VM runs each function as C++ wherever the game or bytecode calls it. Shooter's Edge's mod server runs such a program with the classic server's calls typed for C++ (`games/shooters_edge/rules/calls.hpp`), and the test mod written this way plays exactly the match its assembled form plays. This is the first step of carrying the game's own rules out of QC.
