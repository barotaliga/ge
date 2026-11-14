// A SAVE'S PICTURE (docs/saves.md § Pictures of a save): the optional `PICT` chunk, a small
// picture of the run as it stood (a few hundred pixels wide) in QOI (qoi.hpp). A game makes it
// from its CPU view (`Run::shoot`, headless) or its renderer's last frame, shrinks it with
// `shrink`, and writes it FIRST after the meta, so a menu reads it off the file's front
// (store.hpp keeps its place in the index).
//
// ge:summary A save's picture: the PICT chunk, shrunk and kept as QOI
// ge:keywords picture pict thumbnail qoi shrink box-filter menu save-picture
// ge:skill save
// ge:test tests/save_picture_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "save/file.hpp"

namespace ge::save {

inline constexpr uint32_t kPictTag = tag("PICT");
inline constexpr uint16_t kPictVersion = 1;
// The picture's widest side by default, pixels.
inline constexpr int kPictureSide = 320;

struct SavePicture {
  uint16_t width = 0;
  uint16_t height = 0;
  std::vector<uint8_t> qoi;
};

template <class Io>
void describe(Io& io, SavePicture& p) {
  io(p.width);
  io(p.height);
  io(p.qoi);
}

// `rgba` (width x height) shrunk by a box filter so its widest side is at most `most` (never
// grown), into `out`; `out_w` and `out_h` its size.
void shrink(int width, int height, std::span<const uint8_t> rgba, int most,
            std::vector<uint8_t>& out, int& out_w, int& out_h);

// The picture `rgba` (width x height), shrunk to `most`, as the `PICT` chunk of `file`.
void put_picture(FileWriter& file, int width, int height, std::span<const uint8_t> rgba,
                 int most = kPictureSide);

// The `PICT` chunk of `file` decoded into `rgba`. False with `problem` when there is none or it
// does not decode.
bool read_picture(FileReader& file, int& width, int& height, std::vector<uint8_t>& rgba,
                  std::string& problem);

}  // namespace ge::save
