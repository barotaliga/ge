// The hub labs' browser: a page the hub writes drawn by a headless Chromium into a lab picture.
// The browser is GE_CHROMIUM or the container's /opt/pw-browsers/chromium; where there is none
// the labs say so and draw nothing. A browser's pixels differ machine to machine, so a lab hands
// these pictures to `Lab::shot` as measured.
//
// A page that keeps working after it loads (the web viewer fetches its wasm, then draws on the
// GPU) is drawn by `live_draw`: the browser is driven over its DevTools pipe (fds 3 and 4, one
// JSON message ending in a NUL each way), the page is opened, the lab waits the given wall-clock
// time, and the screenshot is asked for then. The command line's `--screenshot` cannot do that:
// `--timeout` cuts the page's loads short and `--virtual-time-budget` runs ahead of the GPU.
// A `LiveAct` list plays the page's input after the wait (a click, a key, a pointer move, each a
// DevTools `Input` call), then waits again before the shot, so a lab can show a page's controls.
//
// ge:summary Hub labs' headless Chromium: a hub page drawn at a width into a lab picture
// ge:keywords lab hub browser chromium headless screenshot page width dark-mode stylesheet
// ge:keywords devtools cdp pipe live-page input dispatch-key dispatch-mouse live-act
// ge:skill net labs
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "hub/pages.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"

namespace ge::lab::hub_browser {

// The page with its stylesheet inlined, so a file:// load draws it as the hub serves it.
inline std::string hub_standalone(const std::string& page) {
  const std::string link = "<link rel=\"stylesheet\" href=\"/style.css\">";
  std::string out = page;
  const size_t at = out.find(link);
  if (at != std::string::npos) {
    out.replace(at, link.size(), "<style>" + ge::hub::hub_stylesheet() + "</style>");
  }
  return out;
}

// The browser's path, or "" when there is none.
inline std::string find_browser() {
  if (const char* env = std::getenv("GE_CHROMIUM"); env && *env) return env;
  std::error_code ec;
  if (std::filesystem::exists("/opt/pw-browsers/chromium", ec)) return "/opt/pw-browsers/chromium";
  return "";
}

// One page drawn at width x height (the dark scheme when `dark`) through files in `dir`; empty
// when the browser failed.
inline std::optional<ge::lab::Picture> hub_draw(const std::string& browser,
                                                const std::filesystem::path& dir,
                                                const std::string& name, const std::string& page,
                                                int width, int height, bool dark) {
  const std::filesystem::path html = dir / (name + ".html"), png = dir / (name + ".png");
  {
    std::ofstream f(html, std::ios::binary);
    f << hub_standalone(page);
  }
  const std::string cmd = "\"" + browser +
                          "\" --headless --no-sandbox --disable-gpu --hide-scrollbars " +
                          (dark ? "--force-dark-mode " : "") +
                          "--force-device-scale-factor=1 --window-size=" + std::to_string(width) +
                          "," + std::to_string(height) + " --screenshot=\"" + png.string() +
                          "\" \"file://" + html.string() + "\" > /dev/null 2>&1";
  if (std::system(cmd.c_str()) != 0) return std::nullopt;
  return ge::lab::read_png(png.string());
}

namespace detail {

// The value of the first `"key":"..."` in a DevTools message, or "".
inline std::string json_string(const std::string& msg, const std::string& key) {
  const std::string open = "\"" + key + "\":\"";
  const size_t at = msg.find(open);
  if (at == std::string::npos) return "";
  const size_t from = at + open.size(), to = msg.find('"', from);
  return to == std::string::npos ? "" : msg.substr(from, to - from);
}

inline std::vector<uint8_t> base64_decode(const std::string& text) {
  std::vector<uint8_t> out;
  out.reserve(text.size() / 4 * 3);
  uint32_t acc = 0;
  int bits = 0;
  for (const char c : text) {
    int v = -1;
    if (c >= 'A' && c <= 'Z')
      v = c - 'A';
    else if (c >= 'a' && c <= 'z')
      v = c - 'a' + 26;
    else if (c >= '0' && c <= '9')
      v = c - '0' + 52;
    else if (c == '+')
      v = 62;
    else if (c == '/')
      v = 63;
    if (v < 0) continue;
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((acc >> bits) & 0xffu));
    }
  }
  return out;
}

#ifndef _WIN32
// The browser's end of the DevTools pipe: messages written whole, read up to a deadline.
struct Pipe {
  int to = -1, from = -1;
  std::string pending;

  bool send(const std::string& msg) {
    const std::string bytes = msg + std::string(1, '\0');
    size_t done = 0;
    while (done < bytes.size()) {
      const ssize_t n = ::write(to, bytes.data() + done, bytes.size() - done);
      if (n <= 0) return false;
      done += static_cast<size_t>(n);
    }
    return true;
  }

  // Reads until a message holding `"id":<id>,` arrives (returned) or the deadline passes (""). An
  // id of 0 only drains what the browser sends until the deadline.
  std::string until(int id, std::chrono::steady_clock::time_point deadline) {
    const std::string tag = "\"id\":" + std::to_string(id) + ",";
    for (;;) {
      for (size_t end; (end = pending.find('\0')) != std::string::npos;) {
        std::string msg = pending.substr(0, end);
        pending.erase(0, end + 1);
        if (id > 0 && msg.find(tag) != std::string::npos) return msg;
      }
      const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                            deadline - std::chrono::steady_clock::now())
                            .count();
      if (left <= 0) return "";
      pollfd p{from, POLLIN, 0};
      if (::poll(&p, 1, static_cast<int>(left)) <= 0) continue;
      char buf[65536];
      const ssize_t n = ::read(from, buf, sizeof buf);
      if (n <= 0) return "";
      pending.append(buf, static_cast<size_t>(n));
    }
  }
};
#endif

}  // namespace detail

// One input to a live page: a DevTools `Input` method and its parameters as JSON, then how long
// to wait after it, ms. `click`, `key` and `move` make the common ones.
struct LiveAct {
  std::string method, params;
  int after_ms = 0;
};
inline std::vector<LiveAct> click(int x, int y, int after_ms = 0) {
  const std::string at = "\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y);
  return {
      {"Input.dispatchMouseEvent",
       "{\"type\":\"mousePressed\",\"button\":\"left\",\"clickCount\":1," + at + "}", 0},
      {"Input.dispatchMouseEvent",
       "{\"type\":\"mouseReleased\",\"button\":\"left\",\"clickCount\":1," + at + "}", after_ms}};
}
inline LiveAct move(int x, int y, int after_ms = 0) {
  return {
      "Input.dispatchMouseEvent",
      "{\"type\":\"mouseMoved\",\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y) + "}",
      after_ms};
}
// `code` is the key's `KeyboardEvent.code` ("Space", "ArrowRight"), `key` what it types.
inline std::vector<LiveAct> key(const std::string& code, const std::string& key_name,
                                int after_ms = 0) {
  const std::string k = "\"code\":\"" + code + "\",\"key\":\"" + key_name + "\"";
  return {{"Input.dispatchKeyEvent", "{\"type\":\"keyDown\"," + k + "}", 0},
          {"Input.dispatchKeyEvent", "{\"type\":\"keyUp\"," + k + "}", after_ms}};
}

// A live page drawn after `wait_ms` of wall-clock time at width x height: the browser started
// with `flags` (its WebGPU switches, say) and driven over its DevTools pipe; its output goes to
// `log`. Empty where the browser failed, answered nothing in time, or on Windows (no pipe here).
inline std::optional<ge::lab::Picture> live_draw(const std::string& browser,
                                                 const std::vector<std::string>& flags,
                                                 const std::string& url, int width, int height,
                                                 int wait_ms, const std::filesystem::path& png,
                                                 const std::filesystem::path& log,
                                                 const std::vector<LiveAct>& acts = {}) {
#ifdef _WIN32
  (void)browser, (void)flags, (void)url, (void)width, (void)height, (void)wait_ms, (void)png,
      (void)log, (void)acts;
  return std::nullopt;
#else
  int down[2], up[2];
  if (::pipe(down) != 0) return std::nullopt;
  if (::pipe(up) != 0) {
    ::close(down[0]), ::close(down[1]);
    return std::nullopt;
  }
  std::vector<std::string> args = {
      browser,
      "--headless=new",
      "--no-sandbox",
      "--hide-scrollbars",
      "--remote-debugging-pipe",
      "--force-device-scale-factor=1",
      "--window-size=" + std::to_string(width) + "," + std::to_string(height)};
  args.insert(args.end(), flags.begin(), flags.end());
  args.push_back("about:blank");
  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (std::string& a : args) argv.push_back(a.data());
  argv.push_back(nullptr);
  const pid_t pid = ::fork();
  if (pid < 0) return std::nullopt;
  if (pid == 0) {
    // The child: the browser reads fd 3 and writes fd 4.
    const int out = ::open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out >= 0) ::dup2(out, 1), ::dup2(out, 2);
    const int r = ::dup(down[0]), w = ::dup(up[1]);
    ::dup2(r, 3), ::dup2(w, 4);
    for (int fd = 5; fd < 256; ++fd) ::close(fd);
    ::execv(argv[0], argv.data());
    ::_exit(127);
  }
  ::close(down[0]), ::close(up[1]);
  detail::Pipe pipe{down[1], up[0], {}};
  using clock = std::chrono::steady_clock;
  const auto soon = [] { return clock::now() + std::chrono::seconds(30); };
  std::optional<ge::lab::Picture> pic;
  std::string target, session;
  if (pipe.send(R"({"id":1,"method":"Target.createTarget","params":{"url":")" + url + "\"}}")) {
    target = detail::json_string(pipe.until(1, soon()), "targetId");
  }
  if (!target.empty() &&
      pipe.send(R"({"id":2,"method":"Target.attachToTarget","params":{"targetId":")" + target +
                R"(","flatten":true}})")) {
    session = detail::json_string(pipe.until(2, soon()), "sessionId");
  }
  if (!session.empty()) {
    pipe.until(0, clock::now() + std::chrono::milliseconds(wait_ms));
    int id = 10;
    for (const LiveAct& a : acts) {
      if (!pipe.send("{\"id\":" + std::to_string(id) + R"(,"sessionId":")" + session +
                     R"(","method":")" + a.method + R"(","params":)" + a.params + "}"))
        break;
      pipe.until(id++, soon());
      pipe.until(0, clock::now() + std::chrono::milliseconds(a.after_ms));
    }
    if (pipe.send(R"({"id":3,"sessionId":")" + session +
                  R"(","method":"Page.captureScreenshot","params":{"format":"png"}})")) {
      // A software GPU may still be building the page's first frame: give the shot two minutes.
      const auto shot_by = clock::now() + std::chrono::minutes(2);
      const std::vector<uint8_t> bytes =
          detail::base64_decode(detail::json_string(pipe.until(3, shot_by), "data"));
      if (!bytes.empty()) {
        std::ofstream f(png, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        f.close();
        pic = ge::lab::read_png(png.string());
      }
    }
  }
  pipe.send(R"({"id":4,"method":"Browser.close"})");
  pipe.until(4, clock::now() + std::chrono::seconds(5));
  ::close(pipe.to), ::close(pipe.from);
  int status = 0;
  if (::waitpid(pid, &status, WNOHANG) == 0) {
    ::kill(pid, SIGKILL);
    ::waitpid(pid, &status, 0);
  }
  return pic;
#endif
}

}  // namespace ge::lab::hub_browser
