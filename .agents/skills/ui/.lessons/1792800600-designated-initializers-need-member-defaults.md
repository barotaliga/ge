---
title: Designated initializers warn on members without a default under -Wextra; give each one `{}`
date: 2026-10-23
scope: engine/ui/menu.hpp
concepts: [designated-initializer, warning, werror, aggregate]
---

`Item` is built with designated initializers (`{.id = "play", .text = "PLAY"}`) in tests and
games. GCC's `-Wmissing-field-initializers` (on with `-Wextra`, an error under `-Werror`) flags
every member left out that has no default member initializer, a `std::string` included. Give
every member of an aggregate meant for designated initialization a default (`std::string id{};`)
so a caller names only what it sets.
