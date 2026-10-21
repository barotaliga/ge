# gameengine2

A C++20 game engine that is fast, deterministic to the bit, and built for agents. Its
first game and demo is **Fall Line**, a downhill skiing game, which lives in the tree
(`games/fall_line/`).

The code is written by AI agents. Start at [AGENTS.md](AGENTS.md).

- [docs/goals.md](docs/goals.md): the goals and the milestone roadmap
- [docs/architecture.md](docs/architecture.md): the layers, the conventions and the decisions

## Build

Requires CMake 3.25+, Ninja and a C++20 compiler (GCC 13, Clang 17, Apple Clang or
MSVC 2022).

```sh
make test               # debug build with sanitizers, then ctest
make test PRESET=release
```
