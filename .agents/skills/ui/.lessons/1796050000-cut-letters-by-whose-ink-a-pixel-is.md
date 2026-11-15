---
title: Cut embossed letters out by whose ink each pixel is, never as boxes, and lay them on metal
date: 2026-11-14
scope: games/shooters_edge/menu/plate_letters.cpp
concepts: [carved-letters, ink, alpha, ownership, pieced-letters, tags, dirty-metal]
---

Letters cut as rectangles of the picture they sit on drag their neighbours' serifs and shadows in
(a T with a bit of the U, a J with the U's left stem) and show as boxes on any other ground. Cut
them as see-through ink instead: bright (face and highlight, with a face's dimmer pixels next to a
bright one grown in) and dark (shadow) pixels are ink; bright parts belong to the cell holding
their middle (split at the cell's edge only when two cells each hold 15 px of the part); each
shadow to the letter of the nearest bright pixel; shadows cut off from their letter's bright ones
are nobody's. A letter keeps only its own ink and grows a rim of the ground around it (two px whole,
then fading); a rim pixel that is another letter's ink takes the ground's mean colour. Piece
missing letters from the cells' pixels (with what each pixel is) and grow the rim after the
pieces are laid, so cut ends get a rim too; drop crumbs under 6 px a cut leaves. Faces in this
art are dimmer than 52, so a fixed bright threshold alone loses thin strokes (the V's left one).
Prototype the cut in Python over the extracted pictures and look at it zoomed on magenta before
writing C++: four rounds of that cost less than one round of C++ and a lab.

A serif's tip often crosses the cell's edge, so splitting a part at the edge hands the tip to the
next letter: it shows as a dot beside that letter (a U with a speck at its foot that was the M's
serif). Give bright parts under 15 px to the nearest big part's letter, and give any piece of a
letter under 10 px that touches another letter's pixels to that letter. Piece a diagonal from a
lit stroke that runs the same way (Z from the Y's right arm, twice) rather than mirroring or
shearing one: a mirrored or sheared stroke loses its light and reads as a smear.
