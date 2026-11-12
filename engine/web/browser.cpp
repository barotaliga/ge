// The browser's side of a web viewer, defined (browser.hpp). Built only for Emscripten.
//
// ge:summary The web viewer's page calls defined: fetch, the paint, the canvas, the status line
// ge:keywords web browser emscripten em-async-js fetch request-animation-frame
// ge:skill net
#include "web/browser.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <cstdlib>

namespace {

// 0 with the bytes in a malloc'd block; the HTTP status when it was not 2xx; -1 when no answer.
EM_ASYNC_JS(int, ge_web_fetch, (const char* url, uint8_t** out, int* len), {
  try {
    const r = await fetch(UTF8ToString(url));
    if (!r.ok) return r.status;
    const b = new Uint8Array(await r.arrayBuffer());
    const p = _malloc(b.length || 1);
    HEAPU8.set(b, p);
    HEAPU32[out >> 2] = p;
    HEAP32[len >> 2] = b.length;
    return 0;
  } catch (e) {
    return -1;
  }
});

// The JS is written without arrow functions: the C++ formatter would split `=>`.
EM_ASYNC_JS(double, ge_web_paint, (),
            { return await new Promise(function(done) { requestAnimationFrame(done); }); });

EM_JS(void, ge_web_say, (const char* line), {
  const t = UTF8ToString(line);
  const e = document.getElementById("viewer-status");
  if (e) e.textContent = t;
  if (t) console.log(t);
});

}  // namespace

namespace ge::web {

bool fetch_bytes(const std::string& url, std::vector<uint8_t>& out, std::string& error) {
  uint8_t* data = nullptr;
  int len = 0;
  const int status = ge_web_fetch(url.c_str(), &data, &len);
  if (status != 0) {
    error = status < 0 ? "the hub did not answer for " + url
                       : "the hub answered " + std::to_string(status) + " for " + url;
    return false;
  }
  out.assign(data, data + len);
  std::free(data);
  return true;
}

double next_frame() { return ge_web_paint(); }

void canvas_size(const char* selector, uint32_t& width, uint32_t& height) {
  int w = 0, h = 0;
  emscripten_get_canvas_element_size(selector, &w, &h);
  width = static_cast<uint32_t>(w > 0 ? w : 0);
  height = static_cast<uint32_t>(h > 0 ? h : 0);
}

void say(const std::string& line) { ge_web_say(line.c_str()); }

}  // namespace ge::web
