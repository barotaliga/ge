# THE 2D LAYER's dependencies (engine/ui, the `ui` skill): stb_truetype (public domain or MIT,
# one header) to read and rasterize TrueType fonts, and the engine's default UI font, Barlow
# Condensed (SIL Open Font License 1.1) at two weights, each a pinned file checked against its
# SHA-256. Only engine/ui/font.cpp includes stb_truetype and compiles its implementation, so no
# public header names it; the fonts are compiled into ge_ui as bytes (cmake/EmbedBinary.cmake), so
# a game reads no font from disk. The font's licence travels with it: engine/ui/fonts/OFL.txt.
#
# Moving a pin: change the commit and the hash together, then rerun tests/ui_font_test.cpp and
# the `menu` lab (its pictures' digests move with the glyphs).
include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/FetchFile.cmake)

set(GE_STB_COMMIT "003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5")
set(GE_STB_TRUETYPE_SHA256 "ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab")
set(GE_STB_INCLUDE "${GE_DEPS_DIR}/stb-${GE_STB_COMMIT}")
ge_fetch_file("stb_truetype.h"
  "https://raw.githubusercontent.com/nothings/stb/${GE_STB_COMMIT}/stb_truetype.h"
  ${GE_STB_TRUETYPE_SHA256} "${GE_STB_INCLUDE}/stb_truetype.h")

set(GE_FONTS_COMMIT "e7db6ca603fd4862010383ff17ffd6cc4ffac1c0")
set(GE_FONTS_DIR "${GE_DEPS_DIR}/fonts-${GE_FONTS_COMMIT}")
set(_ge_fonts_url "https://raw.githubusercontent.com/google/fonts/${GE_FONTS_COMMIT}/ofl/barlowcondensed")
ge_fetch_file("Barlow Condensed SemiBold" "${_ge_fonts_url}/BarlowCondensed-SemiBold.ttf"
  "7b619d14bc2327509a9ef32b0890f709626f7ecc9ff61191c2a4314c5499d2d9"
  "${GE_FONTS_DIR}/BarlowCondensed-SemiBold.ttf")
ge_fetch_file("Barlow Condensed Medium" "${_ge_fonts_url}/BarlowCondensed-Medium.ttf"
  "262bd143292ce479ee0cd09a42b47ab173fca8e9c6eb5ed0b5c8a845bc371d17"
  "${GE_FONTS_DIR}/BarlowCondensed-Medium.ttf")
