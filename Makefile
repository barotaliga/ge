# The everyday commands. CMake is the build; this file only names its calls.
# PRESET=debug (default: sanitizers on) or PRESET=release.
PRESET ?= debug
# Tracked AND new (untracked, not ignored) files, so a file not yet `git add`ed is formatted too.
SOURCES := $(shell git ls-files --cached --others --exclude-standard '*.cpp' '*.hpp' 2>/dev/null)

.PHONY: build test fmt fmt-check lint agents-check index find lessons labs lab sim play labs-smoke \
	labs-digests libm-check release-check shaders-check clean

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

# Every embedded WGSL shader compiled by Tint offline, every entry point (cmake/Wgsl.cmake): a
# shader error fails here with its file and line, no GPU needed. Needs GE_GPU (Dawn's tint).
shaders-check:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET) --target shaders_check

agents-check:
	tools/check-agents.sh

libm-check:
	tools/check-libm.sh

# The labs (docs/labs.md, skill: labs). `labs` lists them; `lab` runs one:
#   make lab LAB=level ARGS="--seeds=38,7"     (ARGS=--help for its flags)
# `labs-smoke` is CI's labs_smoke case; `labs-digests` rewrites its baseline (a reviewed change).
LABS = build/$(PRESET)/labs
LAB ?=
labs: build
	$(LABS)

lab: build
	$(LABS) $(LAB) $(ARGS)

# Fall Line's headless sim CLI is the `sim` lab: make sim SEEDS=1,2,38 (empty: the reference seeds).
SEEDS ?=
sim: build
	$(LABS) sim $(if $(SEEDS),--seeds=$(SEEDS)) $(ARGS)

# The play protocol on stdio: the build's output goes to stderr, so stdout is the protocol alone.
play:
	@$(MAKE) --no-print-directory build >&2
	@$(LABS) play --quiet --stdio $(ARGS)

labs-smoke: build
	$(LABS) --all --smoke --repeat=2 --quiet --check-digests=tests/labs_digests.txt

labs-digests: build
	$(LABS) --all --smoke --quiet --write-digests=tests/labs_digests.txt

# Discovery (AGENTS.md § Finding things). `index` rewrites docs/index.md from every file's ge:
# tags; `find` searches the files, the skills and their lessons: make find Q="noise heightfield".
# `lessons` reads the skills' lessons: make lessons ARGS="port --scope=engine/core".
index:
	node tools/agents-index.mjs --write

find:
	node tools/agents-index.mjs --find $(Q)

lessons:
	node tools/skill-lessons.mjs $(ARGS)

# The porting working area's targets (make golden, make golden-libm), while it exists.
-include porting/porting.mk

# The release plumbing (docs/publishing.md): fragments valid, tools tested,
# and the version the next release would get (none right after a release).
release-check:
	python3 -m unittest tools/release_test.py
	python3 tools/release.py check
	@echo "next release: $$(python3 tools/release.py bump 2>/dev/null || echo 'none (no fragments in .changes/unreleased/)')"

clean:
	rm -rf build
