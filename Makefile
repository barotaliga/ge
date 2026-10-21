# The everyday commands. CMake is the build; this file only names its calls.
# PRESET=debug (default: sanitizers on) or PRESET=release.
PRESET ?= debug
# Tracked AND new (untracked, not ignored) files, so a file not yet `git add`ed is formatted too.
SOURCES := $(shell git ls-files --cached --others --exclude-standard '*.cpp' '*.hpp' 2>/dev/null)

.PHONY: build test fmt fmt-check lint agents-check index find lessons golden golden-libm libm-check release-check clean

build:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)

test: build
	ctest --preset $(PRESET)

fmt:
	clang-format -i $(SOURCES)

fmt-check:
	clang-format --dry-run --Werror $(SOURCES)

lint: build
	clang-tidy -p build/$(PRESET) --quiet $(filter %.cpp,$(SOURCES))

agents-check:
	tools/check-agents.sh

libm-check:
	tools/check-libm.sh

# Discovery (AGENTS.md § Finding things). `index` rewrites docs/index.md from every file's ge:
# tags; `find` searches the files, the skills and their lessons: make find Q="noise heightfield".
# `lessons` reads the skills' lessons: make lessons ARGS="port --scope=engine/core".
index:
	node tools/agents-index.mjs --write

find:
	node tools/agents-index.mjs --find $(Q)

lessons:
	node tools/skill-lessons.mjs $(ARGS)

# Rewrites tests/golden/core_* (bar quat and solar), loop_*, core_js* and racing_* from the
# TypeScript oracle (needs Node 22+ and checkouts of oss-game-framework and game5 beside this
# one, or FRAMEWORK=path GAME5=path; no npm install in either).
FRAMEWORK ?= ../oss-game-framework
GAME5 ?= ../game5
golden:
	node --experimental-strip-types --disable-warning=ExperimentalWarning tools/parity/golden-core.mjs --framework $(FRAMEWORK)
	node tools/parity/golden-js.mjs
	node --experimental-strip-types --disable-warning=ExperimentalWarning tools/parity/golden-racing.mjs --framework $(FRAMEWORK) --game5 $(GAME5)

# Rewrites tests/golden/libm_*.txt, core_quat.txt, core_solar.txt and mapgen_generate.txt:
# everything that reaches a transcendental. Needs game5's pinned Node (NODE=path, ../game5/.nvmrc) on Linux x86-64 with
# glibc; the scripts refuse anything else and say why (tools/parity/oracle-node.mjs).
NODE ?= node
golden-libm:
	$(NODE) tools/parity/golden-libm.mjs --game5 $(GAME5)
	$(NODE) --experimental-strip-types --disable-warning=ExperimentalWarning tools/parity/golden-core-libm.mjs --game5 $(GAME5) --framework $(FRAMEWORK)
	$(NODE) --experimental-strip-types --disable-warning=ExperimentalWarning tools/parity/golden-mapgen.mjs --game5 $(GAME5) --framework $(FRAMEWORK)

# The release plumbing (docs/publishing.md): fragments valid, tools tested,
# and the version the next release would get (none right after a release).
release-check:
	python3 -m unittest tools/release_test.py
	python3 tools/release.py check
	@echo "next release: $$(python3 tools/release.py bump 2>/dev/null || echo 'none (no fragments in .changes/unreleased/)')"

clean:
	rm -rf build
