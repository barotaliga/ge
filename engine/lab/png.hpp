// THE PNG WRITER the labs save pictures with: RGBA8, deflate in STORED blocks (no compression,
// no dependency), the CRC-32 and the Adler-32 the format needs. A lab picture is a few
// megabytes at most, and a compression dependency is not worth the saving. Any viewer, and an
// agent's image reader, opens it.
//
// ge:summary The labs' PNG writer: RGBA8, stored deflate, CRC-32 and Adler-32
// ge:keywords png encode image file crc32 adler32 deflate screenshot
// ge:skill labs
// ge:test tests/lab_raster_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "lab/raster.hpp"

namespace ge::lab {

uint32_t crc32(const uint8_t* bytes, size_t n, uint32_t crc = 0);

std::vector<uint8_t> encode_png(const Picture& picture);

// Write `picture` to `path`; false (and nothing half-written left behind) if it could not.
bool write_png(const std::string& path, const Picture& picture);

}  // namespace ge::lab
