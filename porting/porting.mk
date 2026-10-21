# THE PORTING TARGETS. A working file: porting/ is deleted when porting ends, and the root
# Makefile includes this only while it exists.
.PHONY: golden golden-libm

# Rewrites tests/golden/core_* (bar quat and solar), loop_*, core_num* and racing_* from the
# TypeScript oracle (needs Node 22+ and checkouts of oss-game-framework and game5 beside this
# one, or FRAMEWORK=path GAME5=path; no npm install in either).
FRAMEWORK ?= ../oss-game-framework
GAME5 ?= ../game5
golden:
	node --experimental-strip-types --disable-warning=ExperimentalWarning porting/parity/golden-core.mjs --framework $(FRAMEWORK)
	node porting/parity/golden-num.mjs
	node --experimental-strip-types --disable-warning=ExperimentalWarning porting/parity/golden-racing.mjs --framework $(FRAMEWORK) --game5 $(GAME5)

# Rewrites tests/golden/libm_*.txt, core_quat.txt, core_solar.txt and mountain_generate.txt:
# everything that reaches a transcendental. Needs game5's pinned Node (NODE=path, ../game5/.nvmrc) on Linux x86-64 with
# glibc; the scripts refuse anything else and say why (porting/parity/oracle-node.mjs).
NODE ?= node
golden-libm:
	$(NODE) porting/parity/golden-libm.mjs --game5 $(GAME5)
	$(NODE) --experimental-strip-types --disable-warning=ExperimentalWarning porting/parity/golden-core-libm.mjs --game5 $(GAME5) --framework $(FRAMEWORK)
	$(NODE) --experimental-strip-types --disable-warning=ExperimentalWarning porting/parity/golden-mapgen.mjs --game5 $(GAME5) --framework $(FRAMEWORK)
