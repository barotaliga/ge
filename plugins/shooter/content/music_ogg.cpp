// AN OGG VORBIS MUSIC FILE DECODED, for the soundtrack store (soundtrack.hpp): a store's install
// keeps the music this way. The only file that includes stb_vorbis (public domain or MIT, pinned by
// commit and SHA-256 in plugins/shooter/CMakeLists.txt), decoding from memory to 16-bit samples.
// Not simulation code: it runs once, when a game's music is taken in.
//
// ge:summary Decodes an Ogg Vorbis music file to 16-bit samples (stb_vorbis) for the soundtracks
// ge:keywords ogg vorbis decode music soundtrack stb_vorbis decode_ogg
// ge:skill shooter audio
// ge:test tests/shooter_content_import_test.cpp
#include <cstdlib>
#include <span>
#include <string>

#include "shooter/content/soundtrack.hpp"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-value"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include "stb_vorbis.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace ge::shooter::content {

bool decode_ogg(std::span<const uint8_t> bytes, Samples* out, std::string* error) {
  int channels = 0, rate = 0;
  short* pcm = nullptr;
  const int frames =
      stb_vorbis_decode_memory(bytes.data(), int(bytes.size()), &channels, &rate, &pcm);
  if (frames < 0 || pcm == nullptr) {
    *error =
        "the Ogg Vorbis file does not decode; it is damaged or not Vorbis, take it again from "
        "its source";
    return false;
  }
  out->channels = channels;
  out->rate = rate;
  out->bits = 16;
  out->data.assign(pcm, pcm + size_t(frames) * size_t(channels));
  std::free(pcm);
  return true;
}

}  // namespace ge::shooter::content
