# The everyday commands. CMake is the build; this file only names its calls.
# PRESET=debug (default: sanitizers on) or PRESET=release.
PRESET ?= debug
SOURCES := $(shell git ls-files '*.cpp' '*.hpp' 2>/dev/null)

.PHONY: build test fmt fmt-check lint agents-check golden clean

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

# Rewrites tests/golden/* from the TypeScript oracle (needs Node 22+ and a
# checkout of oss-game-framework beside this one, or FRAMEWORK=path).
FRAMEWORK ?= ../oss-game-framework
golden:
	node --experimental-strip-types --disable-warning=ExperimentalWarning tools/parity/golden-core.mjs --framework $(FRAMEWORK)

clean:
	rm -rf build
