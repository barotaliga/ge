---
title: The Mac's debug preset and its gates are run by no CI row; Apple's tools break what Linux passes
date: 2026-10-30
scope: tools/setup-macos.sh, tools/check-labs.mjs, tests/fall_line_lift_ride_test.cpp
concepts: [macos, apple-clang, debug, git-grep, regex, unused-const-variable, setup]
---

CI's macOS row builds only the release preset and runs no gate, so on a Mac two things failed that
every Linux row passes. Apple Clang's `-Wunused-const-variable` fires on a namespace-scope
`constexpr` used only under `#ifdef NDEBUG` (a golden path read only in release); GCC is silent, so
mark it `[[maybe_unused]]`. And Apple's git uses the system's POSIX regex, where `git grep -E`
takes `\s` as a literal `s` and matches nothing, so a tool reading `git grep` output sees an empty
tree: write `[[:space:]]`, never `\s`, `\w`, `\d` or `\b`, in a `git grep` pattern. BSD `sed -E`
ignores `\b` the same way and edits nothing without an error; use `perl -pi -e` for a word-bounded
replace on a Mac. A full `make lint` on 14 cores takes about ten minutes (about 4 s a file).

The lint on macOS (libc++) sees what Linux's (libstdc++) cannot: libc++ throws from inline header
code, libstdc++ from inside its compiled library, so `bugprone-exception-escape` fires on a Mac
for every destructor, `main` and closure move that can reach an allocation. Those are true on both
platforms; fix them as such: a `main` that wraps a `run_main` in a try with an `fprintf` line, a
destructor that catches `std::exception` and says so on stderr (an empty catch fails
`bugprone-empty-catch`), a lambda that captures a `const` local builds it in the capture instead
(`[p = Pilot(...)]`: a `const` member is copied, not moved, when the closure moves). `size_t` and
`uint64_t` are different types there, so `bugprone-misplaced-widening-cast` fires too: convert
each operand, then compute. The analyzer's `cplusplus.Move` inside `std::sort`'s comparator is a
false positive of its libc++ model: NOLINTBEGIN/END with that reason.

Two more in `tools/lint.sh`'s changed-files path, which a change to the lint script itself skips:
macOS's awk refuses a newline in a `-v` value (pass a list through `ENVIRON`), and Apple Clang's
depfiles list toolchain headers before the source (take the object's first `.cpp` in the tree,
never its first dependency).
