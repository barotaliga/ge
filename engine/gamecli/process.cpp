// The programs started, asked after and stopped, per platform (process.hpp).
//
// ge:summary Detached processes per platform: posix_spawn or CreateProcess, alive, stop
// ge:keywords gamecli process spawn_detached process_alive process_stop self_dir posix-spawn
// ge:keywords createprocess waitpid sigterm
// ge:skill agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include "gamecli/process.hpp"

#include <chrono>
#include <filesystem>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
extern char** environ;
#endif

namespace ge::gamecli {

namespace fs = std::filesystem;

#ifdef _WIN32

namespace {

// One argument quoted as the C runtime reads a command line back.
std::string quoted(const std::string& a) {
  if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos) return a;
  std::string out = "\"";
  size_t slashes = 0;
  for (const char c : a) {
    if (c == '\\') {
      slashes++;
      continue;
    }
    if (c == '"')
      out.append(slashes * 2 + 1, '\\');
    else
      out.append(slashes, '\\');
    slashes = 0;
    out += c;
  }
  out.append(slashes * 2, '\\');
  return out + "\"";
}

}  // namespace

bool spawn_detached(const std::vector<std::string>& argv, const std::string& cwd,
                    const std::string& log, long& pid, std::string& error) {
  if (argv.empty()) {
    error = "nothing to start; next: name the program";
    return false;
  }
  std::string line;
  for (const std::string& a : argv) line += (line.empty() ? "" : " ") + quoted(a);
  SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE out = CreateFileA(log.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (out == INVALID_HANDLE_VALUE) {
    error = "the log " + log + " cannot be written; next: check the folder's permissions";
    return false;
  }
  STARTUPINFOA si{};
  si.cb = sizeof si;
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = nullptr;
  si.hStdOutput = out;
  si.hStdError = out;
  PROCESS_INFORMATION pi{};
  const BOOL ok = CreateProcessA(nullptr, line.data(), nullptr, nullptr, TRUE,
                                 DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr,
                                 cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
  CloseHandle(out);
  if (!ok) {
    error = argv[0] + " did not start (error " + std::to_string(GetLastError()) +
            "); next: check that the program is there";
    return false;
  }
  pid = static_cast<long>(pi.dwProcessId);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

bool process_alive(long pid) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (h == nullptr) return false;
  DWORD code = 0;
  const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
  CloseHandle(h);
  return alive;
}

bool process_stop(long pid, int, std::string& error) {
  HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
  if (h == nullptr) return true;  // gone already
  const bool ok = TerminateProcess(h, 0) != 0;
  if (ok) WaitForSingleObject(h, 5000);
  CloseHandle(h);
  if (!ok) error = "process " + std::to_string(pid) + " did not stop; next: end it in Task Manager";
  return ok;
}

std::string self_dir(const char* argv0) {
  char buf[MAX_PATH];
  const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  const fs::path p = n > 0 ? fs::path(std::string(buf, n)) : fs::path(argv0 ? argv0 : "");
  return p.has_parent_path() ? p.parent_path().string() : std::string(".");
}

std::string program_path(const std::string& dir, const std::string& name) {
  return (fs::path(dir) / (name + ".exe")).string();
}

#else

bool spawn_detached(const std::vector<std::string>& argv, const std::string& cwd,
                    const std::string& log, long& pid, std::string& error) {
  if (argv.empty()) {
    error = "nothing to start; next: name the program";
    return false;
  }
  // The log opened here so a folder that cannot be written is said before anything starts.
  const int fd = ::open(log.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
  if (fd < 0) {
    error = "the log " + log + " cannot be written; next: check the folder's permissions";
    return false;
  }
  ::close(fd);
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_addopen(&actions, 1, log.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  posix_spawn_file_actions_adddup2(&actions, 1, 2);
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
#ifdef POSIX_SPAWN_SETSID
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);  // its own session: the terminal may close
#endif
  std::vector<char*> args;
  args.reserve(argv.size() + 1);
  for (const std::string& a : argv) args.push_back(const_cast<char*>(a.c_str()));
  args.push_back(nullptr);
  // posix_spawn has no working folder: the folder is changed round the call (this process is
  // single-threaded) and changed back.
  std::error_code ec;
  const fs::path was = fs::current_path(ec);
  if (!cwd.empty()) fs::current_path(cwd, ec);
  if (ec) {
    error = "the folder " + cwd + " cannot be entered; next: check that it exists";
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    return false;
  }
  pid_t child = 0;
  const int rc = posix_spawn(&child, args[0], &actions, &attr, args.data(), environ);
  if (!cwd.empty()) fs::current_path(was, ec);
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attr);
  if (rc != 0) {
    error = argv[0] + " did not start (error " + std::to_string(rc) +
            "); next: check that the program is there and runs";
    return false;
  }
  pid = static_cast<long>(child);
  return true;
}

bool process_alive(long pid) {
  if (pid <= 0) return false;
  int status = 0;
  if (::waitpid(static_cast<pid_t>(pid), &status, WNOHANG) == pid) return false;  // ours, reaped
  return ::kill(static_cast<pid_t>(pid), 0) == 0;
}

bool process_stop(long pid, int grace_ms, std::string& error) {
  if (!process_alive(pid)) return true;
  if (::kill(static_cast<pid_t>(pid), SIGTERM) != 0) {
    error = "process " + std::to_string(pid) +
            " may not be stopped by this user; next: stop it as the user who started it";
    return false;
  }
  for (int waited = 0; waited < grace_ms; waited += 20) {
    if (!process_alive(pid)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  ::kill(static_cast<pid_t>(pid), SIGKILL);
  for (int waited = 0; waited < 2000; waited += 20) {
    if (!process_alive(pid)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  error = "process " + std::to_string(pid) + " did not stop; next: kill -9 " + std::to_string(pid);
  return false;
}

std::string self_dir(const char* argv0) {
  std::error_code ec;
#if defined(__APPLE__)
  char buf[4096];
  uint32_t size = sizeof buf;
  if (_NSGetExecutablePath(buf, &size) == 0) {
    const fs::path p = fs::weakly_canonical(fs::path(buf), ec);
    if (!ec) return p.parent_path().string();
  }
#else
  const fs::path p = fs::read_symlink("/proc/self/exe", ec);
  if (!ec) return p.parent_path().string();
#endif
  const fs::path a = fs::absolute(fs::path(argv0 ? argv0 : ""), ec);
  return ec || !a.has_parent_path() ? std::string(".") : a.parent_path().string();
}

std::string program_path(const std::string& dir, const std::string& name) {
  return (fs::path(dir) / name).string();
}

#endif

}  // namespace ge::gamecli
