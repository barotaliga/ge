// A LOCAL PIPE: a byte stream to another program on the same machine, by name. A Unix domain
// socket at a filesystem path on macOS, Linux, iOS and Android; a named pipe (`\\.\pipe\<name>`)
// on Windows. Non-blocking like the sockets: `local_pump` moves what the OS will take and what it
// has, and nothing ever blocks a frame. The bytes are the caller's: a protocol frames them. It is
// how a game reaches a desktop client's local IPC (the Discord link, docs/services.md § Discord).
// A listener is the other end, for tests and labs that stand in for the client.
//
// ge:summary A local IPC byte stream by name: a Unix domain socket or a Windows named pipe
// ge:keywords net local pipe ipc unix domain socket named pipe af-unix stream non-blocking
// ge:keywords listen accept connect discord-ipc
// ge:skill net services
// ge:test tests/net_local_pipe_test.cpp
#pragma once

#include <cstdint>
#include <string>

namespace ge::net {

struct LocalPipe {
  intptr_t handle = -1;
  bool closed = false;  // the other end closed it or the OS failed it
  std::string out;      // bytes the OS has not taken yet
  std::string in;       // bytes read and not yet consumed by the caller
};

struct LocalListener {
  intptr_t handle = -1;
  std::string path;
};

// The OS's name for a local pipe called `name`: `<dir>/<name>` for a socket, `\\.\pipe\<name>` on
// Windows (where `dir` is ignored).
std::string local_pipe_path(const std::string& dir, const std::string& name);

// Connect to the pipe at `path`; false and `error` when nothing listens there.
bool local_connect(LocalPipe& p, const std::string& path, std::string* error);
void local_close(LocalPipe& p);
// Queue bytes; they go on the next pump. False when the pipe is closed.
bool local_send(LocalPipe& p, const std::string& bytes);
// Move bytes both ways. A pipe the other end closed, or the OS failed, is marked `closed`.
void local_pump(LocalPipe& p);

// Listen at `path` (a stale socket file there is replaced); false and `error` when the OS refuses.
bool local_listen(LocalListener& l, const std::string& path, std::string* error);
void local_close_listener(LocalListener& l);
// The next connection waiting on the listener; false when none is.
bool local_accept(LocalListener& l, LocalPipe& out);

// This process's id, as a local client asks for it.
int64_t local_process_id();

}  // namespace ge::net
