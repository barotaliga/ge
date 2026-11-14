// The picture shrunk, encoded into its chunk and read back.
//
// ge:summary The PICT chunk written and read; the box-filter shrink
// ge:keywords picture pict shrink box-filter qoi
// ge:skill save
// ge:test tests/save_picture_test.cpp
#include "save/picture.hpp"

#include <algorithm>

#include "save/qoi.hpp"

namespace ge::save {

void shrink(int width, int height, std::span<const uint8_t> rgba, int most,
            std::vector<uint8_t>& out, int& out_w, int& out_h) {
  const int side = std::max(width, height);
  const int k = side <= most ? 1 : (side + most - 1) / most;  // whole pixels a box, each way
  out_w = std::max(1, width / k);
  out_h = std::max(1, height / k);
  out.assign(static_cast<size_t>(out_w) * static_cast<size_t>(out_h) * 4, 0);
  for (int y = 0; y < out_h; y++)
    for (int x = 0; x < out_w; x++)
      for (int c = 0; c < 4; c++) {
        int sum = 0, n = 0;
        for (int dy = 0; dy < k && y * k + dy < height; dy++)
          for (int dx = 0; dx < k && x * k + dx < width; dx++) {
            sum += rgba[(static_cast<size_t>(y * k + dy) * static_cast<size_t>(width) +
                         static_cast<size_t>(x * k + dx)) *
                            4 +
                        static_cast<size_t>(c)];
            n++;
          }
        out[(static_cast<size_t>(y) * static_cast<size_t>(out_w) + static_cast<size_t>(x)) * 4 +
            static_cast<size_t>(c)] = static_cast<uint8_t>((sum + n / 2) / std::max(n, 1));
      }
}

void put_picture(FileWriter& file, int width, int height, std::span<const uint8_t> rgba, int most) {
  std::vector<uint8_t> small;
  int w = 0, h = 0;
  shrink(width, height, rgba, most, small, w, h);
  SavePicture p;
  p.width = static_cast<uint16_t>(w);
  p.height = static_cast<uint16_t>(h);
  qoi_encode(w, h, small, p.qoi);
  file.put(kPictTag, 0, kPictVersion, p);
}

bool read_picture(FileReader& file, int& width, int& height, std::vector<uint8_t>& rgba,
                  std::string& problem) {
  SavePicture p;
  if (!file.get(kPictTag, 0, kPictVersion, p)) {
    problem = file.find(kPictTag) == nullptr ? "the save has no picture" : file.problem();
    return false;
  }
  return qoi_decode(p.qoi, width, height, rgba, problem);
}

}  // namespace ge::save
