# gameengine2

A C++20 game engine built to run **Fall Line**, a downhill skiing game, natively and
fast. Fall Line is ported from its TypeScript original
([game5](https://github.com/niclaslindstedt/game5)) and stays in the tree as the
engine's demo game. The generic parts are ported from
[oss-game-framework](https://github.com/niclaslindstedt/oss-game-framework).

The code is written by AI agents. Start at [AGENTS.md](AGENTS.md).

- [docs/goals.md](docs/goals.md): the goals and the milestone roadmap
- [docs/research.md](docs/research.md): engine research and decisions
- [docs/architecture.md](docs/architecture.md): the layers and conventions
- [docs/porting.md](docs/porting.md): how TypeScript becomes bit-exact C++

## Build

Requires CMake 3.25+, Ninja and a C++20 compiler (GCC 13, Clang 17, Apple Clang or
MSVC 2022).

```sh
make test               # debug build with sanitizers, then ctest
make test PRESET=release
```
