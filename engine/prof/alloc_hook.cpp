// The global operator new and delete, replaced to count (memory.hpp). An executable links this
// file by calling `count_allocations()`; each replacement counts and then calls malloc or free as
// the standard library's own would, so a sanitizer still sees every block and its leaks.
//
// ge:summary The counting replacement of the global operator new and delete (count_allocations)
// ge:keywords prof memory allocations operator-new operator-delete hook count_allocations
// ge:skill perf
// ge:test tests/prof_zones_test.cpp
#include <atomic>
#include <cstdlib>
#include <new>

#include "prof/memory.hpp"

namespace ge::prof {

namespace detail {
extern std::atomic<bool> g_counting;
}  // namespace detail

void count_allocations() { detail::g_counting.store(true, std::memory_order_relaxed); }

}  // namespace ge::prof

namespace {

void* counted(std::size_t n) {
  ge::prof::detail::note_alloc(n);
  return std::malloc(n == 0 ? 1 : n);
}

void* counted_aligned(std::size_t n, std::align_val_t a) {
  ge::prof::detail::note_alloc(n);
  const auto align = static_cast<std::size_t>(a);
#if defined(_WIN32)
  return _aligned_malloc(n == 0 ? 1 : n, align);
#else
  // aligned_alloc wants a size that is a multiple of the alignment.
  const std::size_t size = ((n == 0 ? 1 : n) + align - 1) / align * align;
  return std::aligned_alloc(align, size);
#endif
}

void release(void* p) {
  if (!p) return;
  ge::prof::detail::note_free();
  std::free(p);
}

void release_aligned(void* p) {
  if (!p) return;
  ge::prof::detail::note_free();
#if defined(_WIN32)
  _aligned_free(p);
#else
  std::free(p);
#endif
}

}  // namespace

// NOLINTBEGIN(misc-new-delete-overloads): the full set is replaced, sized and aligned forms too.
void* operator new(std::size_t n) {
  if (void* p = counted(n)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
  if (void* p = counted(n)) return p;
  throw std::bad_alloc();
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return counted(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return counted(n); }
void* operator new(std::size_t n, std::align_val_t a) {
  if (void* p = counted_aligned(n, a)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n, std::align_val_t a) {
  if (void* p = counted_aligned(n, a)) return p;
  throw std::bad_alloc();
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
  return counted_aligned(n, a);
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
  return counted_aligned(n, a);
}
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p, std::size_t) noexcept { release(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete(void* p, std::align_val_t) noexcept { release_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { release_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { release_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { release_aligned(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
  release_aligned(p);
}
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
  release_aligned(p);
}
// NOLINTEND(misc-new-delete-overloads)
