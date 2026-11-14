// The moon's face fetched by a web build's viewer when a night sky first asked for it
// (render/moon.hpp § A web build); compiled only with GE_WEB.
//
// ge:summary A web viewer fetches the moon's face file from the hub when the sky asks for it
// ge:keywords render moon lazy-moon moon-face-file fetch_moon_face web-download
// ge:skill render
#include <cstdio>
#include <string>
#include <vector>

#include "render/moon.hpp"
#include "web/browser.hpp"

namespace ge::render {

void fetch_moon_face() {
  if (!take_moon_face_want()) return;
  std::vector<uint8_t> bytes;
  std::string error;
  const std::string url = std::string("/viewer/") + kMoonFaceFile;
  if (!web::fetch_bytes(url, bytes, error)) {
    std::fprintf(stderr, "viewer: the moon's face did not load, so the moon is drawn plain: %s\n",
                 error.c_str());
    return;
  }
  if (!set_moon_face(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size())))
    std::fprintf(stderr, "viewer: %s was not a whole moon map, so the moon is drawn plain\n",
                 url.c_str());
}

}  // namespace ge::render
