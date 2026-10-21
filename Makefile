# The everyday commands. CMake is the build; this file only names its calls.
# PRESET=debug (default: sanitizers on) or PRESET=release.
PRESET ?= debug
SOURCES := $(shell git ls-files '*.cpp' '*.hpp' 2>/dev/null)

.PHONY: build test fmt fmt-check lint agents-check golden golden-libm libm-check clean

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

# Rewrites tests/golden/core_* (bar quat and solar), loop_*, core_js and racing_* from the
# TypeScript oracle (needs Node 22+ and checkouts of oss-game-framework and game5 beside this
# one, or FRAMEWORK=path GAME5=path; no npm install in either).
FRAMEWORK ?= ../oss-game-framework
GAME5 ?= ../game5
golden:
	node --experimental-strip-types --disable-warning=ExperimentalWarning tools/parity/golden-core.mjs --framework $(FRAMEWORK)
	node tools/parity/golden-js.mjs
	node --experimental-strip-types --disable-warning=ExperimentalWarning tools/parity/golden-racing.mjs --framework $(FRAMEWORK) --game5 $(GAME5)

# Rewrites tests/golden/libm_*.txt, core_quat.txt and core_solar.txt: everything that reaches a
# transcendental. Needs game5's pinned Node (NODE=path, ../game5/.nvmrc) on Linux x86-64 with
# glibc; the scripts refuse anything else and say why (tools/parity/oracle-node.mjs).
NODE ?= node
golden-libm:
	$(NODE) tools/parity/golden-libm.mjs --game5 $(GAME5)
	$(NODE) --experimental-strip-types --disable-warning=ExperimentalWarning tools/parity/golden-core-libm.mjs --game5 $(GAME5) --framework $(FRAMEWORK)

clean:
	rm -rf build
