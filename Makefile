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

# Rewrites tests/golden/* from the TypeScript oracle (needs Node 22+ and a
# checkout of oss-game-framework beside this one, or FRAMEWORK=path).
FRAMEWORK ?= ../oss-game-framework
golden:
	node --experimental-strip-types --disable-warning=ExperimentalWarning tools/parity/golden-core.mjs --framework $(FRAMEWORK)
	node tools/parity/golden-js.mjs

# Rewrites tests/golden/libm_*.txt. Needs game5's pinned Node (NODE=path, ../game5/.nvmrc)
# on Linux x86-64 with glibc; the script refuses anything else and says why.
NODE ?= node
GAME5 ?= ../game5
golden-libm:
	$(NODE) tools/parity/golden-libm.mjs --game5 $(GAME5)

clean:
	rm -rf build
