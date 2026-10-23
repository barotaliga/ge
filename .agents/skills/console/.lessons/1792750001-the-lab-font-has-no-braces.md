---
title: The labs' 5x7 font draws no braces, so a console line built with them shows blanks
date: 2026-10-23
scope: engine/console/view.cpp, engine/lab/labs/console_view_lab.cpp
concepts: [console, lab-font, glyphs, nearby, say_prox, picture]
---

The `console` lab draws text with the lab raster's 5x7 font, which has no `{` or `}`: a
proximity line written `{name}: text` came out as ` NAME : TEXT`, and the digest did not
change, so only looking at the picture caught it. Prefix a line kind with characters the font
draws (`(near) name: text`), and look at `previews/console/down.png` after any change to how
a line is written.
