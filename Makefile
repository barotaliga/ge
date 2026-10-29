# The everyday commands. CMake is the build; this file only names its calls.
# PRESET=debug (default: sanitizers on) or PRESET=release.
PRESET ?= debug
# Tracked AND new (untracked, not ignored) files, so a file not yet `git add`ed is formatted too.
SOURCES := $(shell git ls-files --cached --others --exclude-standard '*.cpp' '*.hpp' 2>/dev/null)

.PHONY: build test fmt fmt-check lint agents-check index find lessons labs lab sim play labs-smoke \
	labs-digests libm-check release-check shaders-check precheck ge clean

build:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)

# Every test case in parallel, one per core (CTEST_PARALLEL_LEVEL overrides). T=<regex> runs only
# the tests it names and builds only their targets (each test is its own executable).
T ?=
test:
	cmake --preset $(PRESET)
	@if [ -n "$(T)" ]; then \
	  targets=$$(ctest --preset $(PRESET) -N -R "$(T)" | sed -n 's/^ *Test *#[0-9]*: *//p' | sed 's/^labs_smoke.*/labs/' | sort -u); \
	  [ -n "$$targets" ] || { echo "make test: no test matches T=$(T); ctest --preset $(PRESET) -N lists them." >&2; exit 1; }; \
	  cmake --build --preset $(PRESET) --target $$targets; \
	else cmake --build --preset $(PRESET); fi
	ctest --preset $(PRESET) $(if $(T),-R "$(T)") -j $${CTEST_PARALLEL_LEVEL:-$$(nproc 2>/dev/null || sysctl -n hw.ncpu)}

fmt:
	clang-format -i $(SOURCES)

fmt-check:
	clang-format --dry-run --Werror $(SOURCES)

# clang-tidy's bug-finding checks, in parallel. BASE=origin/main lints only what the change since
# that ref can reach (CI's lint on a pull request): make lint BASE=origin/main.
BASE ?=
lint: build
	tools/lint.sh build/$(PRESET) $(BASE)

# Every embedded WGSL shader compiled by Tint offline, every entry point (cmake/Wgsl.cmake): a
# shader error fails here with its file and line, no GPU needed. Needs GE_GPU (Dawn's tint).
shaders-check:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET) --target shaders_check

agents-check:
	tools/check-agents.sh

# Before every push: CI's cheap gates in order, stopping at the first red one (format, agents,
# libm, fragments, build, lint and the tests the change reaches since BASE, default origin/main).
precheck:
	tools/precheck.sh build/$(PRESET) $(PRESET) $(or $(BASE),origin/main)

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

# The play protocol on stdio (Fall Line's `play` executable, engine/play/main.hpp): the build's
# output goes to stderr, so stdout is the protocol alone. ARGS="--seed=38 --rivals=3", or a
# command: ARGS="sim --seeds=38", ARGS="replay runs/fall_line-38.inputs".
play:
	@$(MAKE) --no-print-directory build >&2
	@build/$(PRESET)/play $(ARGS)

labs-smoke: build
	$(LABS) --all --smoke --repeat=2 --quiet --check-digests=tests/digests

labs-digests: build
	$(LABS) --all --smoke --quiet --write-digests=tests/digests

# Discovery (AGENTS.md § Finding things). `index` writes docs/index.md (gitignored) from every file's ge:
# tags; `find` searches the files, the skills and their lessons: make find Q="noise heightfield".
# `lessons` reads the skills' lessons: make lessons ARGS="port --scope=engine/core". `index` also
# writes docs/lab-list.md (gitignored), the labs' table, from every LabSpec.
index:
	node tools/agents-index.mjs --write
	node tools/check-labs.mjs

find:
	node tools/agents-index.mjs --find $(Q)

lessons:
	node tools/skill-lessons.mjs $(ARGS)

# The ge command line (engine/cli, skill: agent-interface), the tool game repos run:
#   make ge ARGS="new ../snow-racer"     scaffold a game repo against this engine's version
# A game repo runs its own (`make agents-check` there is `ge doctor`).
ge: build
	build/$(PRESET)/ge $(ARGS)

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
