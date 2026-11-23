// A PICTURE WITH ITS NOTES: a frame and the words that explain it in one image, for a report pasted
// to an agent or a person (`gpudebug`, capture/debug_shot.hpp). The frame keeps every pixel; a band
// is added below it holding the lines, set in a monospace charset (the console's `mono`), each
// wrapped to the picture's width at a space where it can be; a line starting "# " is a heading,
// drawn in its own colour with a rule above it.
//
// The text's size follows the picture's width (`sheet_text_size`: 16 px a 1280 px of width, in
// whole steps of the font's grid), so the words stay readable when a viewer scales a large frame
// down to its own size. Drawn by the CPU raster (raster.hpp): the same pixels on every machine.
//
// ge:summary A frame with its notes set below it in one picture, for a report an agent reads
// ge:keywords debug-sheet notes caption band report screenshot gpudebug wrap monospace
// ge:keywords debug_sheet sheet_text_size
// ge:skill ui
// ge:test tests/ui_debug_sheet_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ui/image.hpp"
#include "ui/paint.hpp"

namespace ge::ui {

struct SheetStyle {
  std::string_view font = "mono";  // a charset's name (charset_font.hpp)
  float size = 0;                  // the text's px; 0: by the picture's width (sheet_text_size)
  Color band = hex(0x111318);      // the band's fill
  Color text = hex(0xe6e6e6);
  Color heading = hex(0xffc24d);  // a "# " line's
  Color rule = hex(0x3a3e48);     // the line above a heading
};

// The text's px for a picture `width` wide: 16 for every 1280 px of it, at least 16.
float sheet_text_size(int width);

// `rgba` (`width` x `height`, rows top to bottom) with `lines` set in a band below it.
Image debug_sheet(int width, int height, std::span<const uint8_t> rgba,
                  std::span<const std::string> lines, const SheetStyle& style = {});

}  // namespace ge::ui
