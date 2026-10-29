---
title: A menu's root page shows the style's logo, not its own title
date: 2026-10-24
scope: engine/ui/menu_view.hpp, games/fall_line/app/menus.hpp
concepts: [menu, root-page, logo, pause-page, menu-style]
---

`MenuView` draws `MenuStyle::logo` on the root page and the page's `title` only on pages opened
over it. A pause menu built as its own `Menu`, with "PAUSED" as its root's title, therefore came
out with the game's name over RESUME. Give each menu that has its own root a style of its own,
with its own logo (Fall Line's `Shell::pause_style`), and lay out and check its pages with that
style.
