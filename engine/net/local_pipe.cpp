// Local pipes over each OS's API: Unix domain sockets (os_socket.hpp's headers and sys/un.h) and
// Windows named pipes in byte mode, read only what PeekNamedPipe says is waiting.
//
// ge:summary Local pipes: Unix domain sockets on POSIX, named pipes on Windows, non-blocking
// ge:keywords net local pipe unix socket sockaddr-un named pipe createnamedpipe peeknamedpipe
// ge:skill net services
// ge:test tests/net_local_pipe_test.cpp
#include "net/local_pipe.hpp"

#include "net/os_socket.hpp"

#if !defined(_WIN32)
#include <sys/un.h>
#endif

namespace ge::net {

namespace {

bool fail(std::string* error, std::string line) {
  if (error) *error = std::move(line);
  return false;
}

}  // namespace

#if defined(_WIN32)

std::string local_pipe_path(const std::string& /*dir*/, const std::string& name) {
  return "\\\\.\\pipe\\" + name;
}

namespace {
HANDLE pipe_of(intptr_t h) { return reinterpret_cast<HANDLE>(h); }
}  // namespace

bool local_connect(LocalPipe& p, const std::string& path, std::string* error) {
  local_close(p);
  p = LocalPipe{};
  HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                         nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return fail(error, "local pipe: nothing listens at " + path + " (OS error " +
                           std::to_string(GetLastError()) +
                           "); next: start the program that serves it");
  // No waiting: a write takes what the pipe's buffer has room for, the rest goes next pump.
  DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
  SetNamedPipeHandleState(h, &mode, nullptr, nullptr);
  p.handle = reinterpret_cast<intptr_t>(h);
  return true;
}

void local_close(LocalPipe& p) {
  if (p.handle != -1) CloseHandle(pipe_of(p.handle));
  p.handle = -1;
  p.closed = true;
}

void local_pump(LocalPipe& p) {
  if (p.handle == -1 || p.closed) return;
  HANDLE h = pipe_of(p.handle);
  if (!p.out.empty()) {
    DWORD wrote = 0;
    // A full pipe takes nothing and says so with success; a failure is a pipe being closed.
    if (!WriteFile(h, p.out.data(), static_cast<DWORD>(p.out.size()), &wrote, nullptr))
      p.closed = true;
    p.out.erase(0, wrote);
  }
  char chunk[16384];
  while (!p.closed) {
    DWORD waiting = 0;
    if (!PeekNamedPipe(h, nullptr, 0, nullptr, &waiting, nullptr)) {
      p.closed = true;
      break;
    }
    if (waiting == 0) break;
    DWORD got = 0;
    const DWORD want = waiting < sizeof chunk ? waiting : static_cast<DWORD>(sizeof chunk);
    if (!ReadFile(h, chunk, want, &got, nullptr) || got == 0) {
      p.closed = true;
      break;
    }
    p.in.append(chunk, got);
  }
}

bool local_listen(LocalListener& l, const std::string& path, std::string* error) {
  local_close_listener(l);
  HANDLE h = CreateNamedPipeA(path.c_str(), PIPE_ACCESS_DUPLEX,
                              PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, 1, 65536, 65536, 0,
                              nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return fail(error, "local pipe: cannot serve " + path + " (OS error " +
                           std::to_string(GetLastError()) + "); next: pick another name");
  l.handle = reinterpret_cast<intptr_t>(h);
  l.path = path;
  return true;
}

void local_close_listener(LocalListener& l) {
  if (l.handle != -1) CloseHandle(pipe_of(l.handle));
  l.handle = -1;
}

// In no-wait mode ConnectNamedPipe says ERROR_PIPE_CONNECTED once a client is on (and success
// while it is still listening). A named pipe serves one client on its own handle: an accepted
// client takes the listener's handle, and the listener opens a fresh instance for the next.
bool local_accept(LocalListener& l, LocalPipe& out) {
  if (l.handle == -1) return false;
  HANDLE h = pipe_of(l.handle);
  if (ConnectNamedPipe(h, nullptr) || GetLastError() != ERROR_PIPE_CONNECTED) return false;
  out = LocalPipe{};
  out.handle = l.handle;
  l.handle = -1;
  const std::string path = l.path;
  local_listen(l, path, nullptr);
  return true;
}

int64_t local_process_id() { return static_cast<int64_t>(GetCurrentProcessId()); }

#else

using namespace os;

std::string local_pipe_path(const std::string& dir, const std::string& name) {
  if (dir.empty()) return name;
  return dir.back() == '/' ? dir + name : dir + "/" + name;
}

namespace {

bool address_of(const std::string& path, sockaddr_un& out, std::string* error) {
  std::memset(&out, 0, sizeof out);
  out.sun_family = AF_UNIX;
  if (path.empty() || path.size() >= sizeof out.sun_path)
    return fail(error, "local pipe: the path '" + path + "' is empty or longer than " +
                           std::to_string(sizeof out.sun_path - 1) +
                           " bytes; next: use a shorter directory");
  std::memcpy(out.sun_path, path.data(), path.size());
  return true;
}

void no_sigpipe(int h) {
#if defined(SO_NOSIGPIPE)
  int one = 1;
  setsockopt(h, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#else
  (void)h;
#endif
}

}  // namespace

bool local_connect(LocalPipe& p, const std::string& path, std::string* error) {
  local_close(p);
  p = LocalPipe{};
  sockaddr_un addr;
  if (!address_of(path, addr, error)) return false;
  const int h = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (h < 0) return fail(error, "local pipe: " + os_error("socket", last_error()));
  // A local connect is answered at once, so it is made blocking and the socket turned after.
  if (::connect(h, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0) {
    const int e = last_error();
    ::close(h);
    return fail(error, "local pipe: nothing listens at " + path + " (" + os_error("connect", e) +
                           "); next: start the program that serves it");
  }
  no_sigpipe(h);
  set_nonblocking(h);
  p.handle = h;
  return true;
}

void local_close(LocalPipe& p) {
  if (p.handle != -1) ::close(static_cast<int>(p.handle));
  p.handle = -1;
  p.closed = true;
}

void local_pump(LocalPipe& p) {
  if (p.handle == -1 || p.closed) return;
  const int h = static_cast<int>(p.handle);
  size_t sent = 0;
  while (sent < p.out.size()) {
    const auto n = ::send(h, p.out.data() + sent, p.out.size() - sent, kNoSignal);
    if (n < 0) {
      if (!would_block(last_error())) p.closed = true;
      break;
    }
    sent += static_cast<size_t>(n);
  }
  p.out.erase(0, sent);
  char chunk[16384];
  while (true) {
    const auto n = ::recv(h, chunk, sizeof chunk, 0);
    if (n == 0) {
      p.closed = true;
      break;
    }
    if (n < 0) {
      if (!would_block(last_error())) p.closed = true;
      break;
    }
    p.in.append(chunk, static_cast<size_t>(n));
  }
}

bool local_listen(LocalListener& l, const std::string& path, std::string* error) {
  local_close_listener(l);
  sockaddr_un addr;
  if (!address_of(path, addr, error)) return false;
  ::unlink(path.c_str());
  const int h = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (h < 0) return fail(error, "local pipe: " + os_error("socket", last_error()));
  if (::bind(h, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0 ||
      ::listen(h, 4) != 0) {
    const int e = last_error();
    ::close(h);
    return fail(error, "local pipe: cannot serve " + path + " (" + os_error("bind", e) +
                           "); next: check the directory exists and is writable");
  }
  set_nonblocking(h);
  l.handle = h;
  l.path = path;
  return true;
}

void local_close_listener(LocalListener& l) {
  if (l.handle != -1) {
    ::close(static_cast<int>(l.handle));
    ::unlink(l.path.c_str());
  }
  l.handle = -1;
}

bool local_accept(LocalListener& l, LocalPipe& out) {
  if (l.handle == -1) return false;
  const int h = ::accept(static_cast<int>(l.handle), nullptr, nullptr);
  if (h < 0) return false;
  no_sigpipe(h);
  set_nonblocking(h);
  out = LocalPipe{};
  out.handle = h;
  return true;
}

int64_t local_process_id() { return static_cast<int64_t>(::getpid()); }

#endif

bool local_send(LocalPipe& p, const std::string& bytes) {
  if (p.handle == -1 || p.closed) return false;
  p.out += bytes;
  return true;
}

}  // namespace ge::net
