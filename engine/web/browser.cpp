// The browser's side of a web viewer, defined (browser.hpp). Built only for Emscripten.
//
// ge:summary The web viewer's page calls defined: fetch, the paint, the canvas, the status line
// ge:keywords web browser emscripten em-async-js fetch request-animation-frame websocket
// ge:keywords webtransport datagram server-certificate-hashes wt_open wt_pipe
// ge:keywords keys pointer scancode take_input take_play_input pointer-lock mouse-look keyup
// ge:skill net
#include "web/browser.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <cstdint>
#include <cstdlib>
#include <memory>

#include "input/state.hpp"

namespace {

// 0 with the bytes in a malloc'd block; the HTTP status when it was not 2xx; -1 when no answer.
// With a label, the status line and the page's bar (#viewer-bar) follow the download.
EM_ASYNC_JS(int, ge_web_fetch, (const char* url, uint8_t** out, int* len, const char* label), {
  try {
    const r = await fetch(UTF8ToString(url));
    if (!r.ok) return r.status;
    const said = UTF8ToString(label);
    const total = Number(r.headers.get("Content-Length") || 0);
    let b;
    if (said && r.body && total > 0) {
      const line = document.getElementById("viewer-status");
      const bar = document.getElementById("viewer-bar");
      const mb = function(n) { return (n / 1048576).toFixed(1); };
      const reader = r.body.getReader();
      const parts = [];
      let got = 0;
      for (;;) {
        const d = await reader.read();
        if (d.done) break;
        parts.push(d.value);
        got += d.value.length;
        if (line) line.textContent = said + " " + mb(got) + " of " + mb(total) + " MB";
        if (bar) {
          bar.max = total;
          bar.value = Math.min(got, total);
          bar.hidden = false;
        }
      }
      if (bar) bar.hidden = true;
      b = new Uint8Array(got);
      let at = 0;
      for (const part of parts) {
        b.set(part, at);
        at += part.length;
      }
    } else {
      b = new Uint8Array(await r.arrayBuffer());
    }
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

// The page's WebSockets, by number: each one's messages queued as they come, taken in order.
EM_JS(int, ge_web_ws_open, (const char* url), {
  if (!Module.geSockets) Module.geSockets = [];
  const s = {queue : [], state : 0, why : ""};
  try {
    s.ws = new WebSocket(UTF8ToString(url));
  } catch (e) {
    return -1;
  }
  s.ws.binaryType = "arraybuffer";
  s.ws.onopen = function() { s.state = 1; };
  s.ws.onclose = function(e) {
    s.state = 2;
    if (!s.why)
      s.why = e.reason || (e.wasClean ? "the other end closed it" : "the connection failed");
  };
  s.ws.onerror = function() { s.state = 2; };
  s.ws.onmessage = function(m) {
    if (m.data instanceof ArrayBuffer) s.queue.push(new Uint8Array(m.data));
  };
  Module.geSockets.push(s);
  return Module.geSockets.length - 1;
});

EM_JS(int, ge_web_ws_state, (int id), {
  const s = Module.geSockets && Module.geSockets[id];
  return s ? s.state : 2;
});

EM_JS(int, ge_web_ws_send, (int id, const uint8_t* data, int len), {
  const s = Module.geSockets && Module.geSockets[id];
  if (!s || s.state != 1) return 0;
  s.ws.send(HEAPU8.slice(data, data + len));
  return 1;
});

// The next message's length, or -1 when none is queued.
EM_JS(int, ge_web_ws_peek, (int id), {
  const s = Module.geSockets && Module.geSockets[id];
  return s && s.queue.length ? s.queue[0].length : -1;
});

EM_JS(void, ge_web_ws_take, (int id, uint8_t* out), {
  const s = Module.geSockets[id];
  HEAPU8.set(s.queue.shift(), out);
});

EM_JS(char*, ge_web_ws_why, (int id), {
  const s = Module.geSockets && Module.geSockets[id];
  const t = s ? s.why : "no such socket";
  const n = lengthBytesUTF8(t) + 1;
  const p = _malloc(n);
  stringToUTF8(t, p, n);
  return p;
});

EM_JS(void, ge_web_ws_close, (int id), {
  const s = Module.geSockets && Module.geSockets[id];
  if (s && s.ws) s.ws.close();
  if (s) s.state = 2;
});

// The page's WebTransport sessions, by number (docs/hub.md § Playing in a browser): datagrams
// both ways, those that come queued as they come (at most 512 waiting: the oldest go first, as a
// full socket's would) and taken in order. The certificate is trusted by its SHA-256 when one is
// given (64 hex digits: the endpoint's own, engine/webtransport/certificate.hpp).
EM_JS(int, ge_web_wt_open, (const char* url, const char* hash), {
  if (!Module.geSessions) Module.geSessions = [];
  const s = {queue : [], state : 0, why : ""};
  const opts = {};
  const hex = UTF8ToString(hash);
  if (hex.length == 64) {
    const v = new Uint8Array(32);
    for (let i = 0; i < 32; i++) v[i] = parseInt(hex.substr(i * 2, 2), 16);
    opts.serverCertificateHashes = [ {algorithm : "sha-256", value : v} ];
  }
  if (typeof WebTransport == "undefined") return -1;
  try {
    s.t = new WebTransport(UTF8ToString(url), opts);
  } catch (e) {
    return -1;
  }
  const gone = function(e) {
    s.state = 2;
    if (!s.why) s.why = e ? String(e.message || e) : "the session closed";
  };
  s.t.ready
      .then(function() {
        s.state = 1;
        s.w = s.t.datagrams.writable.getWriter();
        const r = s.t.datagrams.readable.getReader();
        const pump = function() {
          r.read()
              .then(function(d) {
                if (d.done) return;
                if (s.queue.length >= 512) s.queue.shift();
                s.queue.push(d.value);
                pump();
              })
              .catch(gone);
        };
        pump();
      })
      .catch(gone);
  s.t.closed.then(function() { gone(null); }).catch(gone);
  Module.geSessions.push(s);
  return Module.geSessions.length - 1;
});

EM_JS(int, ge_web_wt_state, (int id), {
  const s = Module.geSessions && Module.geSessions[id];
  return s ? s.state : 2;
});

EM_JS(int, ge_web_wt_send, (int id, const uint8_t* data, int len), {
  const s = Module.geSessions && Module.geSessions[id];
  if (!s || s.state != 1) return 0;
  s.w.write(HEAPU8.slice(data, data + len)).catch(function(){});
  return 1;
});

// The next datagram's length, or -1 when none is queued.
EM_JS(int, ge_web_wt_peek, (int id), {
  const s = Module.geSessions && Module.geSessions[id];
  return s && s.queue.length ? s.queue[0].length : -1;
});

EM_JS(void, ge_web_wt_take, (int id, uint8_t* out), {
  const s = Module.geSessions[id];
  HEAPU8.set(s.queue.shift(), out);
});

EM_JS(void, ge_web_wt_close, (int id), {
  const s = Module.geSessions && Module.geSessions[id];
  if (s && s.t && s.state != 2) s.t.close();
  if (s) {
    s.state = 2;
    if (!s.why) s.why = "closed by the page";
  }
});

// Why the session closed, in a malloc'd string ("" while it is open).
EM_JS(char*, ge_web_wt_why, (int id), {
  const s = Module.geSessions && Module.geSessions[id];
  const t = s ? s.why : "no such session";
  const n = lengthBytesUTF8(t) + 1;
  const p = _malloc(n);
  stringToUTF8(t, p, n);
  return p;
});

// The canvas's keys, buttons, wheel and pointer, gathered by listeners set on the first call and
// kept in `Module.geIn`: keys by the keyboard's scancode (the USB HID usage the platform layer uses
// too) as they go down and up, the pointer in the canvas's pixels, the mouse's movement while the
// pointer is locked to the canvas (`play`: a click locks it, Escape lets it go). A click focuses
// the canvas, so its keys never scroll the page; losing the focus lets every key and button go. The
// JS keeps to what the C++ formatter leaves alone: no `===`, no regular expressions, no `=>`.
EM_JS(int, ge_web_attach, (const char* selector, int play), {
  const c = document.querySelector(UTF8ToString(selector));
  if (!c) return 0;
  if (c.geInput) {
    if (play) c.geInput.play = 1;
    return 1;
  }
  const g = {
    keys : [],
    ev : [],
    down : {},
    x : -1,
    y : -1,
    b : 0,
    bm : 0,
    mx : 0,
    my : 0,
    wh : 0,
    play : play
  };
  const codes = {
    Enter : 40,
    Escape : 41,
    Backspace : 42,
    Tab : 43,
    Space : 44,
    Minus : 45,
    Equal : 46,
    BracketLeft : 47,
    BracketRight : 48,
    Backslash : 49,
    Semicolon : 51,
    Quote : 52,
    Backquote : 53,
    Comma : 54,
    Period : 55,
    Slash : 56,
    Pause : 72,
    Insert : 73,
    Home : 74,
    PageUp : 75,
    Delete : 76,
    End : 77,
    PageDown : 78,
    ArrowRight : 79,
    ArrowLeft : 80,
    ArrowDown : 81,
    ArrowUp : 82,
    ControlLeft : 224,
    ShiftLeft : 225,
    AltLeft : 226,
    ControlRight : 228,
    ShiftRight : 229,
    AltRight : 230
  };
  const code_of = function(e) {
    let k = codes[e.code];
    const n = e.code.length;
    if (!k && n == 4 && e.code.indexOf("Key") == 0) k = e.code.charCodeAt(3) - 61;
    if (!k && n == 6 && e.code.indexOf("Digit") == 0) k = 30 + (e.code.charCodeAt(5) - 39) % 10;
    if (!k && n >= 2 && n <= 3 && e.code.charAt(0) == "F") {
      const f = Number(e.code.substring(1));
      if (f >= 1 && f <= 12) k = 57 + f;
    }
    return k || 0;
  };
  c.tabIndex = 0;
  c.addEventListener(
      "keydown", function(e) {
        const k = code_of(e);
        if (!k) return;
        e.preventDefault();
        if (e.repeat) return;
        g.keys.push(k);
        g.ev.push(k + 65536);
        g.down[k] = 1;
      });
  c.addEventListener(
      "keyup", function(e) {
        const k = code_of(e);
        if (!k) return;
        e.preventDefault();
        g.ev.push(k);
        delete g.down[k];
      });
  c.addEventListener(
      "blur", function() {
        for (const k in g.down) g.ev.push(Number(k));
        g.down = {};
        g.bm = 0;
      });
  const at = function(e) {
    const r = c.getBoundingClientRect();
    g.x = (e.clientX - r.left) * c.width / r.width;
    g.y = (e.clientY - r.top) * c.height / r.height;
    if (document.pointerLockElement == c) {
      g.mx += e.movementX || 0;
      g.my += e.movementY || 0;
    }
  };
  c.addEventListener("pointermove", at);
  c.addEventListener(
      "pointerdown", function(e) {
        at(e);
        g.b = 1;
        c.focus();
        if (g.play && document.pointerLockElement != c && c.requestPointerLock) {
          const p = c.requestPointerLock();
          if (p && p.catch) p.catch(function(){});
        }
      });
  window.addEventListener("pointerup", function() { g.b = 0; });
  c.addEventListener(
      "pointerleave", function() {
        if (!g.b) g.x = g.y = -1;
      });
  c.addEventListener(
      "mousedown", function(e) {
        if (e.button < 5) g.bm |= 1 << e.button;
      });
  window.addEventListener(
      "mouseup", function(e) {
        if (e.button < 5) g.bm &= ~(1 << e.button);
      });
  c.addEventListener("contextmenu", function(e) { e.preventDefault(); });
  c.addEventListener("wheel",
                     function(e) {
                       if (!g.play) return;
                       e.preventDefault();
                       if (e.deltaY) g.wh += e.deltaY > 0 ? -1 : 1;
                     },
                     {passive : false});
  c.geInput = g;
  Module.geIn = g;
  return 1;
});

// The keys that went down since the last call into `keys` (at most `most`) and the pointer into
// `xyb` (x, y, a button held); the count.
EM_JS(int, ge_web_input, (int* keys, int most, float* xyb), {
  const g = Module.geIn;
  const n = Math.min(g.keys.length, most);
  for (let i = 0; i < n; i++) HEAP32[(keys >> 2) + i] = g.keys[i];
  g.keys = [];
  g.ev = [];
  HEAPF32[xyb >> 2] = g.x;
  HEAPF32[(xyb >> 2) + 1] = g.y;
  HEAPF32[(xyb >> 2) + 2] = g.b;
  return n;
});

// The keys' downs and ups since the last call into `ev` (the scancode, plus 65536 for a down; at
// most `most`) and the mouse into `m` (movement x and y, wheel notches, the buttons' bits, whether
// the pointer is locked); the count.
EM_JS(int, ge_web_play, (int* ev, int most, float* m), {
  const g = Module.geIn;
  const n = Math.min(g.ev.length, most);
  for (let i = 0; i < n; i++) HEAP32[(ev >> 2) + i] = g.ev[i];
  g.ev = g.ev.slice(n);
  g.keys = [];
  HEAPF32[m >> 2] = g.mx;
  HEAPF32[(m >> 2) + 1] = g.my;
  HEAPF32[(m >> 2) + 2] = g.wh;
  HEAPF32[(m >> 2) + 3] = g.bm;
  HEAPF32[(m >> 2) + 4] =
      document.pointerLockElement && document.pointerLockElement.geInput == g ? 1 : 0;
  g.mx = g.my = g.wh = 0;
  return n;
});

}  // namespace

namespace ge::web {

void take_input(const char* selector, PageInput& out) {
  out = {};
  if (!ge_web_attach(selector, 0)) return;
  int keys[64];
  float xyb[3] = {-1, -1, 0};
  const int n = ge_web_input(keys, 64, xyb);
  out.pressed.assign(keys, keys + n);
  out.x = xyb[0];
  out.y = xyb[1];
  out.button = xyb[2] != 0;
}

bool take_play_input(const char* selector, ge::input::InputState& s) {
  ge::input::begin_frame(s);
  if (!ge_web_attach(selector, 1)) return false;
  int ev[128];
  float m[5] = {0, 0, 0, 0, 0};
  const int n = ge_web_play(ev, 128, m);
  for (int i = 0; i < n; i++) {
    const int code = ev[i] & 0xffff;
    const bool down = ev[i] >= 65536;
    if (code <= 0 || code >= ge::input::kKeys) continue;
    const auto k = static_cast<size_t>(code);
    if (down && !s.keys_held[k]) s.keys_pressed[k] = true;
    if (!down && s.keys_held[k]) s.keys_released[k] = true;
    s.keys_held[k] = down;
  }
  s.mouse.dx += m[0];
  s.mouse.dy += m[1];
  s.mouse.raw = true;
  s.mouse.wheel_y += m[2];
  const auto bits = static_cast<uint64_t>(m[3]);
  ge::input::Edges64& b = s.mouse.buttons;
  b.pressed = bits & ~b.held;
  b.released = b.held & ~bits;
  b.held = bits;
  return m[4] != 0;
}

int ws_open(const std::string& url) { return ge_web_ws_open(url.c_str()); }

WsState ws_state(int socket) {
  const int s = ge_web_ws_state(socket);
  return s == 0 ? WsState::connecting : s == 1 ? WsState::open : WsState::closed;
}

bool ws_send(int socket, std::span<const uint8_t> msg) {
  return ge_web_ws_send(socket, msg.data(), static_cast<int>(msg.size())) != 0;
}

bool ws_take(int socket, std::vector<uint8_t>& msg) {
  const int n = ge_web_ws_peek(socket);
  if (n < 0) return false;
  msg.resize(static_cast<size_t>(n));
  ge_web_ws_take(socket, msg.data());
  return true;
}

std::string ws_why(int socket) {
  char* p = ge_web_ws_why(socket);
  std::string why = p;
  std::free(p);
  return why;
}

void ws_close(int socket) { ge_web_ws_close(socket); }

int wt_open(const std::string& url, const std::string& cert_hash) {
  return ge_web_wt_open(url.c_str(), cert_hash.c_str());
}

WtState wt_state(int session) {
  const int s = ge_web_wt_state(session);
  return s == 0 ? WtState::connecting : s == 1 ? WtState::open : WtState::closed;
}

bool wt_send(int session, std::span<const uint8_t> datagram) {
  return ge_web_wt_send(session, datagram.data(), static_cast<int>(datagram.size())) != 0;
}

bool wt_take(int session, std::vector<uint8_t>& datagram) {
  const int n = ge_web_wt_peek(session);
  if (n < 0) return false;
  datagram.resize(static_cast<size_t>(n));
  ge_web_wt_take(session, datagram.data());
  return true;
}

void wt_close(int session) { ge_web_wt_close(session); }

std::string wt_why(int session) {
  char* p = ge_web_wt_why(session);
  std::string why = p;
  std::free(p);
  return why;
}

namespace {

// The session (or the WebSocket) and the address its datagrams are reported from.
struct PipeEnd {
  int session = -1;
  net::Address peer;
  bool socket = false;  // a WebSocket's messages, not a session's datagrams
};

bool pipe_send(void* ctx, const net::Address&, const uint8_t* data, size_t size) {
  const PipeEnd& end = *static_cast<PipeEnd*>(ctx);
  const int n = static_cast<int>(size);
  return (end.socket ? ge_web_ws_send(end.session, data, n)
                     : ge_web_wt_send(end.session, data, n)) != 0;
}

int pipe_receive(void* ctx, uint8_t* buffer, size_t capacity, net::Address& from) {
  const PipeEnd& end = *static_cast<PipeEnd*>(ctx);
  const int id = end.session;
  const int n = end.socket ? ge_web_ws_peek(id) : ge_web_wt_peek(id);
  if (n < 0) return -1;
  const auto take = [&end, id](uint8_t* out) {
    if (end.socket)
      ge_web_ws_take(id, out);
    else
      ge_web_wt_take(id, out);
  };
  if (static_cast<size_t>(n) > capacity) {
    std::vector<uint8_t> drop(static_cast<size_t>(n));  // too big for the caller: dropped
    take(drop.data());
    return -1;
  }
  take(buffer);
  from = end.peer;
  return n;
}

net::DatagramPipe pipe_over(int id, const net::Address& peer, bool socket) {
  // One end a session or a socket, kept for the page's life (a page opens a handful at most).
  static std::vector<std::unique_ptr<PipeEnd>> ends;
  ends.push_back(std::make_unique<PipeEnd>(PipeEnd{id, peer, socket}));
  net::DatagramPipe p;
  p.send = pipe_send;
  p.receive = pipe_receive;
  p.ctx = ends.back().get();
  return p;
}

}  // namespace

net::DatagramPipe wt_pipe(int session, const net::Address& peer) {
  return pipe_over(session, peer, false);
}

net::DatagramPipe ws_pipe(int socket, const net::Address& peer) {
  return pipe_over(socket, peer, true);
}

bool fetch_bytes(const std::string& url, std::vector<uint8_t>& out, std::string& error,
                 const std::string& progress) {
  uint8_t* data = nullptr;
  int len = 0;
  const int status = ge_web_fetch(url.c_str(), &data, &len, progress.c_str());
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
