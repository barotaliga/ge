---
type: Added
title: One script sets up a Windows machine
---

`tools/setup-windows.ps1` takes a Windows 10 or 11 machine to one that builds the engine with MSVC and runs its tests and labs on its own GPU: it installs Visual Studio 2022's Build Tools with the C++ workload (or adds the workload to an existing Build Tools), then by winget Git for Windows, CMake, Ninja, ccache, GNU make, LLVM 18's clang-format and clang-tidy, Python 3, Node.js, gh and nasm; puts a tool its installer left off PATH on the user's PATH, gives `python3` a real Python in place of Windows' Store alias, and checks out again with LF any shell script a CRLF checkout broke. It runs on Windows PowerShell 5.1, takes the same `--check`, `--list`, `--verify` and `--quiet` as the Ubuntu and macOS scripts, installs only what is missing, broken or too old, and calls neither winget nor the network when nothing is. `.gitattributes` now keeps `*.sh` at LF on every checkout.
