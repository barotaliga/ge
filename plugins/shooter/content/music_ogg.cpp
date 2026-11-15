// AN OGG VORBIS MUSIC FILE DECODED, for the soundtrack store (soundtrack.hpp): a store's install
// keeps the music this way. The only file that calls stb_vorbis (public domain or MIT, pinned by
// commit and SHA-256 and built as C by plugins/shooter/CMakeLists.txt), decoding from memory to
// 16-bit samples. Not simulation code: it runs once, when a game's music is taken in.
//
// ge:summary Decodes an Ogg Vorbis music file to 16-bit samples (stb_vorbis) for the soundtracks
// ge:keywords ogg vorbis decode music soundtrack stb_vorbis decode_ogg
// ge:skill shooter audio
// ge:test tests/shooter_content_import_test.cpp
#include <cstdlib>
#include <span>
#include <string>

#include "shooter/content/soundtrack.hpp"

// stb_vorbis's one call this file uses, compiled as C beside the plugin.
extern "C" int stb_vorbis_decode_memory(const unsigned char* mem, int len, int* channels,
                                        int* sample_rate, short** output);

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
