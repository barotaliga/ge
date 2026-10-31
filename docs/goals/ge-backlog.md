# The `ge` backlog

What agents working on the engine and its games said `ge` lacked (AGENTS.md § SAY WHAT `ge`
LACKED), one wish a line under its group: the command, what it would answer, and `done:` with
what was built once it lands. Add a wish as a new line under the group it fits; git merges
this file with its union driver (`.gitattributes`), so threads adding lines side by side never
conflict. Milestone M12d in [`../goals.md`](../goals.md) takes these by group.

## Digests and goldens (done)

- `ge digest <lab> [--write]` (one lab's baseline, not the whole smoke run)
- `ge digest <a> <b> --write` (done: several labs' rows checked or rewritten in one call; the labs executable refuses a second name rather than running only the first)
- `ge golden diff` (which columns of a golden's rows moved) and `ge golden write`
- `ge golden diff <test>` naming the component of a state hash that moved (skier, heli, army, wanted, gunfire), so a golden another PR's change moved is told from a cross-platform break; #279 (done: a parts word beside the digest, diffed part by part)
- `ge digest --diff` (done: every lab's digest against the baseline in one command, nothing written, only the labs that moved printed)
- `ge golden diff` pairing rows by their key when the count changes (done: a row inserted mid-table is one row added)
- `ge golden write` listing every golden file it rewrote, and the engine's own tree pointed at it (`build/<preset>/ge golden write <test>`) rather than setting `GE_GOLDEN_WRITE` by hand
- `ge digest-diff --base=<ref> --versions --regions` (which generator versions and regions a change moved)
- `ge digest --rebase=<ref>` (after a merge, the ref's digest files taken for every lab this change does not reach, only the reached labs rerun and written: a conflict in tests/digests/ settled in one step; multiplayer thread)

## Before and after across refs

- `ge lab run <id> --before=<ref>` (the lab built at `<ref>`, default origin/main, in a scratch worktree sharing the compiler cache, and run tagged `before`, then this tree's run beside it: the before-and-after shots without a manual checkout, rebuild and restore; TAB scoreboard thread; done: `ge lab <id> --before=<ref>`, the worktree under build/before/<sha>/, this build's GE_* options and downloads, ccache's base dir)
- `ge lab pair <lab> --before=<cvars>` (any picture lab's frames drawn twice, once under a changed cvar set, and paired side by side: se-client got a one-off `--before` for this; one flag family with `--before=<ref>`, the before taken from a ref or from a cvar set; relief-maps thread, #388; done: `ge lab pair <id> --before=<cvars> [--after=<cvars>]` over the runner's `--cvars` and `lab.cvars()`, read by the world and Shooter's Edge client, world and models labs)
- `ge lab compare --switch=<cvar>` (any lab's views with a cvar off and on, side by side with the luminance difference: the classic texture rebake thread wrote a PIL script for it twice; done: `ge lab pair <id> --switch=<cvar>`)
- `ge bench ab --base=<ref>` (or `--ab <binA> <binB>`: two builds interleaved N times with statistics, since a container's timings wander by about 5%; for a bench lab too: `ge ab <lab> --before=<binary> --runs=3`, the median of each metric; also `--rounds=3`, each side's best and the delta printed beside the measured noise band, the second binary built or reused off `--before=<ref>`: the approximation study; asked again by the map-build profiling, #307: the means and their spread) and `ge compare <ref>` (both builds, one table)
- `ge bench --ab=<option>` (one frame option against another in the same build, interleaved rounds, the least median kept: a quality ladder priced without a bespoke loop)
- `ge profile --compare` (per zone and per pass, GPU and CPU deltas, outliers flagged)
- `ge lab bench <lab> --runs=N --base=main` (each metric's median and spread over N runs, this tree beside the base)
- `ge digest <lab> --why` (which table row and which columns moved when a digest did; for a lab that hashes the command catalog or another input, name that input: "cli-loop moved because a command or flag was added")
- `ge lab diff` for a lab's tables and `--stash`
- `ge lab sheet <lab> --grid=2x2 [shots]` (shots on one image) and `--watch`
- `ge lab grid <lab> --vary=cfg:a,b,c` (one composite picture of the same view under several settings, each tile captioned with its setting; asked again as `ge lab sheet <lab> --view=<v> --vary=cfg:...`)
- `ge lab run --jobs=N <labs>` (independent labs run in parallel, each with its own log, instead of one after another)
- `ge lab shot <lab> <id>...` / `ge lab <id> --only=<shot>` (one named view of a picture lab drawn alone while a look is iterated, the others left as the last run wrote them, written to `--out=<path>` when given: a runner-wide `--shots=<ids>` and `lab.wants(id)` the lab asks before it renders, in place of the `--views` flag gpu-world and gpu-snow each added and the rest lack; asked for by the console, replays, classic-enhance and lobby threads; done: `--shots=<id,...>` on the runner and `ge lab`, `lab.wants(id)`, the world lab skipping the views not named)
- a shared `--viewports=<w>x<h>,...` flag for picture labs (every view drawn at each size, e.g. a phone, a laptop and a 4K screen, without each lab adding its own)
- `ge profile --pass=<name>` (one pass's median and p95, before and after) and `--cost <shader-term>`
- `ge lab <id> --at-ref=<git-ref>` (the "before" shot from a worktree build of that ref, without stashing, rebuilding and restoring this tree)
- `ge bench --pass=<name>` (one pass timed alone over many draws: the software rasterizer's timestamps misattribute cost between passes)
- `ge lab time <lab> --passes=<glob>` (each matching pass's GPU milliseconds, the median over N frames, from any GPU lab without a bench of its own)
- `ge lab sweep <lab> --flag=<flag>=a,b --rounds=N` (the built sweep, which already runs a lab once per flag value with every metric side by side, run N interleaved rounds with each metric's median and spread: a quality knob priced in one call, where a container's timings wander; asked as `ge lab ab` and `ge lab compare --knob`, and it generalizes `ge net sweep`)
- `ge lab diff <lab> --with=<flag>` (the same shots with and without one pass or option, the share of pixels it changes per distance band: the water labs each hand-roll it)
- `ge lab diff <lab>` (built: every shot against the kept run, PSNR, SSIM and the share changed per distance band off the frame's depth, each band held to its floor, a four-panel sheet with the change heatmap and the bands' contours)

## The CI loop

- the session-start hook failing loudly (done: the package step cut off after `GE_SETUP_TIMEOUT` and tried twice, a worker stopped by a signal writes `failed killed ... during <step>`, and `--wait` names a worker that died outright as `failed died` with its last step, instead of a status left `running`; TAB scoreboard thread)
- precheck running every gate and printing each failure whole (done: no gate stops the run but the build, so every lint finding shows in one round)
- precheck refusing two rows of one lab in `tests/labs_digests.txt` (done, then retired: each lab's digest is its own file under `tests/digests/`, so no union merge can double one)
- `ge precheck` (done: the repo's CI gates in both presets) and `ge lint --like-ci` (CI's exact gates and file set) (done: the release preset, the shaders' check, and each reached file the build does not compile an error instead of linted bare)
- `ge lint` (done: clang-tidy's bug-finding checks over what the change since `--base` reaches, the changed .cpp files and every .cpp including a changed header, or the files named, each finding at its line with its check's page; a few files in seconds)
- `ge lint` with no configured build (configures the compile database it needs on its own, export-compile-commands only, instead of failing)
- a teleport detector for the labs (done: `engine/lab/teleport.hpp`, every step of a track that moves farther than its speed allows, reported under every scenario lab's `--trace`)
- `ge test --affected` (done: the tests the change since `--base` reaches through the include graph, a target reached when its objects include a changed file or a header in a reached library's directory; only their targets built, `--list` names them; precheck takes its tests from it)
- precheck's lab digests following the same reach (done: `ge digest --affected [--list]`, each lab by its own source's object, so a plugin change reaches the game's labs through the game library including its headers; `tools/labs-reached.sh` retired; a changed committed asset reaches the labs under its plugin or game and those naming it)
- precheck's digests under a time budget (a change to engine/lab or engine/core reaches every lab, and the whole set outran a session's time limit: the labs a changed file names first, then the rest sharded in the background or left to CI's labs-smoke with the list printed; a time estimate before it starts, and `--labs=gpu` for a frame change that reaches about 111 labs)
- `make precheck` / `ge precheck --scope=<paths>` / `--tests=<regex>` (a regex naming the tests to run in place of the reached set; only the tests and labs the named paths reach; and a prose-only change, AGENTS.md included, reaching no test or lab at all: an AGENTS.md edit widened one precheck to 275 tests and every lab digest, about 47 minutes; classic game loop thread)
- the labs' digest baseline merging without conflicts (done: `merge=union`, a lab's doubled row failed by name)
- `ge digest`'s smoke run writing its previews apart from a full lab run's (`previews/<id>/smoke/`), so a digest check never overwrites the pictures a full run left
- `ge digest --dedupe` (the duplicate rows a union merge leaves in the baseline dropped, keeping the row the current build writes)
- `docs/goals.md` merging without conflicts (done: each row's status in `docs/goals/<row>.md`, this backlog one wish a line, union-merged, held by `tools/check-goals.mjs`)
- `ge ci <pr>` (done: each failed job's log as findings, over `gh api`; `--merge` squash-merges a green, clean one at its head, and a green PR GitHub blocks is answered with the required check or review it waits on)
- `ge ci <pr>` reading the merge queue: "in the queue since <t>" or "dequeued at <t>: <reason>" (a merge conflict with a batch-mate, a failed row), off the PR's timeline (`added_to_merge_queue`, `removed_from_merge_queue`), so a silent ejection is seen at the next check-in
- `ge test --filter` building only the matching tests' targets (done, and `make test T=<regex>` in the engine)
- lint that also covers the files whose compile command changed (`compile_commands.json` against the base)
- precheck running the smoke digest of every lab the change reaches (done in the engine's `make precheck`; `ge precheck` already runs every lab)
- `labs --all --write-digests` keeping the rows of labs the build does not register (done)
- `ge ci run --row=macos -- <cmd> --repeat=N` (a command on one CI row through a dispatched workflow, no temporary workflow step committed: a Metal flake hunt, the needs-mac bench)
- `ge ci <pr> --rebase-digests` (after merging the base, the digests both sides moved rewritten and named)
- `ge goals next` / `ge goals free-rows` (the next open roadmap row with its dependencies, and which rows open pull requests already claim; the second done: `ge goals free [<row>] [--fetch]`, the rows main and every remote branch hold and the next free number or letter; and `ge goals claim <what> [--under=<row>]`, the row reserved and pushed at once, the earlier of two claims keeping it)
- `ge goals claim <what> --exit=<text> --labs=<lab,...>` (the row's exit criterion and its **Labs it lands** cell written in the same step, so a claimed row file is complete at once; single-player spec thread) (done)
- `ge goals free` / `claim` with no build (done: `node tools/goals-free.mjs [<row>]` and `--claim "<what>" [--under] [--exit] [--labs]`, the same rows read and the same next free row, for a fresh session; dedicated server thread)
- `ge goals split <row> <n>` (a milestone row split into sub-rows in docs/goals.md, each with its status file, numbered as `ge goals free` numbers a sub-row)
- `ge lab sweep <id> --set=<const>=a,b,c` (done: one constexpr rewritten to each value, the labs rebuilt and run, every metric side by side, the source put back; next, a scenario row's own numbers, such as a scripted hand's timings, swept the same way, and `--draws=N` over a range `a..b`, a Monte Carlo of one constant with each metric's spread; and a range form, `--set=<const>=a..b/n`, charting each metric against the knob; and `--sheet`, every value's pictures side by side on one `ge lab publish` page, a contact sheet of the knob)
- `ge ci <pr> --wait[=<minutes>]` (done: once a minute until a check fails, the PR conflicts or every check finished, at most an hour; with `--merge` the green answer is merged)
- `ge lab sweep <id> --flag=<flag>=a,b,c` (done: one of the lab's own flags at each value, the labs built once, the same table; the hours of a day over a picture lab)
- `ge lab --split=N`
- `ge job run\|status\|log` (a long run in the background)
- `ge merge-main` (done: `origin/main` merged in, a file main retired taking main's deletion with what to carry named, a lab digest both sides moved rerun and rewritten, every other conflict named and left, the merge committed once none is left; asked by #80, #276, #284); next, a generated file taken from main and regenerated
- `ge stack <pr>...` and `ge restack` (build on other open PRs' branches, and once they land replay only this branch's own commits onto main, without hand-picking the base for `git rebase --onto`)
- `ge build --lean` (no GPU, no debug info: CPU-only plugin work on a small cloud disk), and precheck / `ge digest --affected` reaching only the tests a cmake/ change touches, not every test executable
- `ge ci --cost[=<runs>]` (runner-minutes and wall time per job over the last N runs, split by event: pull request, merge queue, push; what a workflow change saves, measured)
- a precheck stamp: `make precheck` writes the head it passed on and its gates, the push carries it, and the gates job fails a push whose head has none (once CI stops running Linux, precheck is the only Linux run, so it must be provable)
- `ge queue` and `ge pr clash <n>` (done in #250: the merge queue in merge order by each PR's last queue event, the armed PRs behind it, every PR dirty against main and the failed group runs, all by a real `git merge-tree`; one PR merged onto main and everything ahead of it, the PR ahead that brings a conflict named); next, `ge queue --watch` on a schedule telling each PR's owner when it starts to clash (#250), and each group run's ref checked so a stale run shows (#243)
- `ge test --affected --by-link` (a test counted as reached when it links a library whose `.cpp` changed, not only through the include graph; #257, #277. Checked: #231 changed `games/fall_line/sim/state.hpp`, which the army-steal test includes, so the include reach already took it; the golden moved because #231 and #164 were each prechecked alone and met only on main, which `ge pr clash` and a precheck after `ge merge-main` catch)
- `ge test --affected --budget=<s>` / `ge precheck --budget=<s>` (when the reach is wide, the reached tests nearest the change run first and the run stops at the budget, the rest left to CI and listed: a shared-library change reached 78 tests and about 8 minutes; #110, #115)
- precheck running a lab's full flags, not only its smoke flags, when a change reaches that lab's subject (`ge lab run --full-for=<lab>`: the smoke run stopped short of the moment that moved; #156)
- precheck naming the goldens and digests main already fails (`--known-red`, off main's last CI run), so a red that is main's own is ruled out without a second full run (#267)
- `ge ci <run> --lab-log` (a failed lab's per-view metrics read out of a CI job's log, so a flake names its view without a rerun; #233)
- `ge digest --affected` following the running `ge`'s preset (the release `ge` still ran the debug labs), and `--only-moved-by=<symbol>` (only the labs whose digest a named symbol can move: a `state.hpp` change reached 67 labs, over 25 minutes in debug; #231)
- `ge digest --affected --write` (only the digests the change moved rewritten, each named with why, in place of one lab at a time; #270), and `ge digest --affected` resuming after an interrupt (a container restart lost a half-written set; #279)

## Inspecting a run

- `ge lab <id> --zoom=x,y,w,h` (every shot, or `--zoom=<shot>:x,y,w,h` one, also written cropped to a region and scaled up, so a lab needs no close-up code of its own; Shooter's Edge HUD thread)
- `ge lab record <lab> --frames=N --fps=F --out=<file>` / `ge lab anim <lab> --shots=<glob> --out=<file>` (a lab's frames written and encoded in one step, or its numbered frame shots assembled: an animated GIF on the engine's own encoder, WebP or MP4 later through the capture's encoders; `ge lab publish` inlining those animations; asked for by the explosions and lightning beam threads)
- `ge lab <id> --frames=t1,t2,...` / `--at=<event>+<dt>` (a picture lab rendered at exact times, the poses interpolated between simulation steps: a 120 Hz step moves a falling eye 15 cm and skips the waterline frames; hand-written once in gpu-plunge)
- plugin commands: `ge tool <plugin> -- <args>` (being built by the classic-enhance PR: a plugin's tool executable built and its JSON relayed); next, its commands listed in `ge help` and forwarded without `tool --`; first users `ge level textures <pak> <map> --out=<dir>` and `ge level clone <bsp> --texmap=<file>`, which today hide as lab flags
- `ge lab compare --tile=3x3 <dirA> <dirB> ...` (the method comparison page built from image folders: each image wrap-tiled, nearest-neighbour for a 1x source, a flip between methods per image at full size)
- `ge lab <id> --compare=<tag> --metric=<name>` (one metric across the kept run and this one, a table per scenario or shot, in place of every metric `--compare` prints today)
- `ge lab why <lab> <metric>` (what drove a lab's metric: the scenario steps, series and inputs that moved it most, and against a kept run with `--compare=<tag>` where it parted)
- `ge lab listen <lab> <id>` (a lab's WAVs transcoded to a small web format, Opus in Ogg, for its report page and `ge lab publish`, without ffmpeg by hand)
- `ge record` (done: a run as an MP4, WebM or GIF and a contact sheet of timestamped stills, its resolution, frame rate, speed, range and encoder quality; and `--gpu`, the game's registered renderer drawing each frame headless in a mode, Fall Line's `lit`, `wireframe`, `depth` and `overlay` from a chase camera, a high chase or the named views; `ge shot --gpu` the same still)
- `ge export <replay> --from-mark=<kind>` (a clip cut straight off a replay file's marks, a crash or a jump, without finding its steps by hand)
- the CPU views' `shoot` honouring `--size` (Fall Line's near view always draws 512 px, about 0.8 s a frame in debug, whatever size is asked)
- `ge state --entities=<kind> --at=<t>`
- `ge stage --put=skier --beside=<entity:id> --off=<m> --speed=<m/s>` (the player set down mid-run beside a live entity, at an offset and a speed, to stage a meeting without scripting a run up to it)
- `ge trace <test> --rows=<series> --every=<s>` (one actor's state, a pursuer's say, as a table every N seconds from a scripted test, without adding prints to the test)
- `ge cvar ls --module=<module>` (an engine module's cvars, their defaults and ranges, without a game: today `ge cvar` reads only a game's play console)
- `ge lab <lab> --cfg=<script>` for every lab (any lab run under a cfg script's cvars, the runner applying it, instead of a --cfg flag each lab adds)
- `ge sim --human --compare=<tag>` (the humanness report beside a kept run's, each metric's move)
- `ge sim find <seed> --when "<predicate>"`
- `ge sim --stalls`
- `ge sim --why` (each run's bot decision timeline beside its input log)
- `ge why --compare=<log>` (two logs' bot decisions side by side where they part)
- every scenario lab's per-step trace as a series file `ge graph` charts (done: `--trace` on the `ride`, `sled`, `heli` and `para` labs writes each scenario's every step as `<id>.trace.tsv` and charts it with its events marked and its flights shaded, `engine/lab/series_charts.hpp` the one helper the `trace` lab shares)
- the same scenarios through `ge why --scenario=<lab>:<id> --step=<n>` once a machine pilot records its decisions (a wing's fold, bite and slack)
- `ge sim --outing=<machine> --seed=<n>` with `--trace=<series>` and `--shot` (one free-ride outing traced or pictured without the whole free lab)
- `ge map <seed>` (a seed's weather, fall, sun and day without a lab)
- `--sky=<kind>` on every picture lab (the map's weather overridden)
- `ge log --step --channel`
- `ge probe`
- `ge lab <id> --sunlit` (a warning when a look's pixels sit in the horizon map's shadow, so a shadow change that moves no pixel is caught)
- `ge lab scene <lab> <scene> --plan` (one scene's moment from above without running the whole lab)
- `ge why --agent=1000..1003 --decision=landing` (the decision trace filtered by agent range and decision name)
- `ge lab pick <lab> <shot> --pixel=x,y` (a picture's value at a pixel before the tonemap, the HDR colour and its depth)
- `ge gpu dump <target> --frame=N --decode=<scheme>`
- `ge asset inspect`
- `ge net portmap [--protocol=pcp|natpmp|upnp]` (ask the real home router this machine sits behind to open a port and print each step's answer, then close it: the check the simulated router cannot make)
- `ge net reach --nat=<kind>,<kind>` (one host and client pair through the simulated NATs, the path each attempt took and why, without the whole `net-reach` lab)
- `ge lab refs` (a reference photo fetched to a temp directory, never committed, JPEG turned to PNG, a refs row written; the model-views lab reads only 8-bit PNG)
- `ge lab <lab> --pairs=<refs dir>` (each reference photo beside our shot of its view, one side-by-side sheet a pair, built straight from the refs rows; the texture thread's comparison sheets on the same machinery)
- a reference round in one run: a per-tag list of `tag=cfg:view` the lab runs in one process, each tag under its own cfg and view, in place of a shell loop of runs
- `ge lab view <glb>` (the model-views lab on any model, in one command)
- `ge lab table <lab> --where col=value` (one slice of a lab's table, a weapon at one range, read without re-running the lab or opening the whole report)
- `ge lab publish <id> [--before=<tag>]` (done: the run's pictures resized and quantized onto one self-contained HTML page, captions and metrics beside them, before and after side by side, ready to publish as the report's picture page: every thread now owes one, and the water rows each built it by hand; several labs on one page as `ge lab publish <a> <b>`, the cross-lab sheet the liquids rows asked for; `--title=<name>` done in #246)
- `ge replay <log> --seek=<s>[,<s>...]` (any game's replay sought off its keyframes: each seek's digest against the straight run, the steps and ms it took, the keyframes' bytes; the `replay` lab's check for every game repo, so a game's `Run::clone` is proven without writing a lab)
- `ge lab publish <id> [--before=<tag>]` (the run's pictures resized and quantized onto one self-contained HTML page, captions and metrics beside them, before and after side by side, ready to publish as the report's picture page: every thread now owes one, and the water rows each built it by hand)
- `ge lab <id> --show` (the run's pictures shown inline beside its metrics: an unmoved digest can hide a visual bug)
- `ge lab pixels <lab> <shot> --line=x0,y0,x1,y1 | --histogram` (a picture's values along a line or its colour histogram, for banding and colour checks without decoding the PNG)
- `ge lab strip <scenario>` (a side-view stick filmstrip of a body over time, frame by frame, without a hand-written dumper and image script)
- `ge lab trace-points <scenario> --stage` (each point of a body's velocity after every solver stage, for finding an energy pump without instrumenting a copy of the solver; generalizes to `ge trace --actor=<id> --fields=<a,b>`, and to `ge trace-view <lab> <scenario> --around=<step>`: the steps round one as a table of position, velocity, grounded and events)
- `ge lab <lab> --scenario=<id> --watch` (one scenario re-run and its picture refreshed on every rebuild, while tuning it)
- `ge level info|build|shot <file>` (a level file checked, each room's triangles and build time printed, one view rendered without the whole lab)
- `ge bsp info|ents|shot <file>` (a classic BSP map's lumps and counts, its entities as a table, one view rendered, without the levels lab)
- `ge bsp probe <pak> <map> [--at=<x,y,z>]` (a map's sky and liquid surfaces, each with its level, centre and area, and a point's leaf contents and column: camera views picked without putting the eye in a wall)
- `ge texture show <recipe> [--ref=<photo>]` (one texture recipe baked and shown at its sizes and tilings, beside a reference photo when given, without the textures lab)
- `ge texture why <name>` (a recipe's likeness_distance split into its parts, the look numbers, the lines and the repeat, so a score can be argued with; it blows up on near-black textures, 1.9 for a visual match)
- `ge texture match --sweep-seeds=<a..b> --only=<name>` (a pinned recipe tried at every seed in a range, each seed's measured evidence in one table: courses a tile, pieces a course, the line's direction, the colour mismatches; `ge lab sweep` reaches a lab's flags, not this command, so today it is a shell loop)
- `ge lab probe <id> --at=<time>` (a lab's frame records printed around one moment, a match's say, without dumping the whole trace)
- `ge lab diff --mask=<family>` (only the pixels one instance family covers compared, so a change to the figures is shown to touch nothing else; #78) and `ge lab diff --summary` (one line a view with its worst band, in place of the full band table; #225)
- `ge graph <lab> --scenario=<id> --cols=a,b` (a scenario's trace read from `previews/<lab>/<id>.trace.tsv` without naming the path; #96)
- `--look=sun|moon[:lens]` on every picture lab (done for every lab over Fall Line's named views as `<view>@sun[:lens]` and `<view>@moon[:lens]`, on `ge::render::aim_at`, and in the `gpu-bsp` lab as `--view=<map>:x,y,z,sun|moon[,lens]`; #187, #217, #222)
- the world lab's aims: `--aim=cloud` (the nearest cloud of a layer in frame, for a sparse kind; #183), `--ski-through=<layer>` (the eye above, at the top of, inside and under a layer; #234), `--wind-across` (the wind set across a view's heading, so a sequence shows it moving; #214)
- `ge lab shelter <map> <point>` (whether a point is open to a fall at a slant, and which face stops it; #203)
- `ge record --gpu --modes=lit,wireframe` (several modes side by side on one contact sheet, frame for frame; #85)
- `ge lab <id> --stage=<pass>:<off>` (one stage of a pass switched off for a run, a layer's glow or its compression, without editing the shader; #224)
- `ge save check --describe-coverage` (every field of a state struct its `describe` leaves out, read from the struct: today a missing field shows only when a save parts from the straight run; #276)
- `ge texture match --round=<n>` keeping each round's verdicts and printing what moved against the last round (#259)
- `ge probe <file>` (a capture's container, tracks and fragments read with the engine's own readers, without ffprobe; #271)
- `ge lab sheet <id> --pairs` (a lab's off and on shots tiled into one before-and-after image; #272)

## Measuring

- `ge lab <gpu-lab>` printing the frame's per-pass GPU and CPU costs as a table beside its metrics
- `ge lab sound <lab>` (a spectrogram of each WAV a lab writes, on its report page: the cut-off falling with distance seen without listening; the classic-enhance sound page's spectrogram moved into engine/lab and reused; also `ge lab listen <id> <wav>` for one WAV: its meters printed (peak, RMS, band, dropouts) and its spectrogram drawn, so an agent with no speakers checks a voice without writing a chart)
- `ge profile --wall` (a sampling CPU profiler: wall time by inlined frame, the main thread apart from the workers, where instruction counts hide a slow call; also `--main-thread <lab>`, each stage split into its time inside parallel_bands and its time alone, in place of a frame-pointer build, a signal-handler sampler and an addr2line script: #307)
- `ge profile --callgrind <lab>` (also as `ge hot <lab> [--callers=<fn>]`: the self and inclusive ranking and one function's top callers in one table; the approximation study)
- `ge mapgen-bench --tree=<worktree>` (only the mountain plugin and core built from a worktree into a scratch directory and the generator timed, so variants are compared without rebuilding every target; the approximation study)
- `ge lab <id> --toggle=<feature>` (a lab's passes timed with the feature on and off, one table)
- `ge issue needs-mac --bench=<invocation>` (the needs-mac issue written with the exact GPU bench command and what to paste back, after trying CI's macOS runner on Metal first, since software-rendered timings say little about real cost)
- `ge mem --where` (the allocating call stack)
- `ge build --flags`
- `ge spans <lab>` (a lab's spans file read back as a table)
- `ge sanitize thread <tests>` (a ThreadSanitizer build of the tests that run threads)
- `ge lab time <id> [--smoke]` (a lab's sections timed, and its smoke run, so a lab that slows CI is found by name)
- `ge bench --preset=release <lab>` (only the labs target built in release, for a lab's release-build cost without a whole release build)
- `ge precheck --gpu` (a GPU build configured beside the default one, lavapipe installed when missing, and what the change reaches in engine/render and a game's app built, linted and tested: a GPU-less container's precheck skips them entirely; #312)
- a bench lab warns when the machine is loaded (load average over the cores: a concurrent ctest made thread starts look ten times slower)
- `ge tsan <test>` (one test and its sources built with ThreadSanitizer, without a whole sanitizer preset)
- `ge ref ephemeris <body> --from=<t> --to=<t> --site=<lat,lon,m>` (a published reference ephemeris fetched once into a test table, and a core function such as the moon's place diffed against it, worst error in arc minutes)
- `ge sky when --seed=<n> --body=<sun|moon|io|...> [--sky=<type>]` (where a sky body stands, altitude and azimuth, and its phase, per day and hour of a seed's sky: a lab's shot time found in one call instead of a throwaway scan test)
- `ge gpu probe --lab=<id> --shot=<name> --pixel=x,y --expr=<wgsl>` (a uniform or a shader expression read back at one pixel of a lab shot, instead of patching the shader with select() outputs and rebuilding)
- `ge load --tag=<name>` / `--compare=<name>` (two load runs' efficiency, lanes' shares and the median frame's bound side by side, so a threading change proves what it spread)
- `ge load --frame=<n>` (the lanes chart zoomed round one frame, the slowest by default, with its critical path as a table)
- `ge load --live` (the load monitor over the game's own window as a player drives it, once the desktop game steps by hand: M19c)
- `ge lab <lab> --rates=60,120,240` (a scenario lab run at several step rates, a metric that moves past its tolerance flagged: a step-rate bug in one command; #107)
- `ge lab run --realtime-report` (a frame-pace chart of any GPU lab's own loop, so "nothing stalled" needs no hand-written chart; #205)
- `ge lab sweep <lab> --flag=a,b,c --seeds=1..N` (a grid of flags and seeds, the combinations that fail tabled: 144 runs looped by hand; #245)

## Hygiene

- a lab that reads the user's own content (a pak) marked private in its spec, so the runner writes it under the local cache instead of previews/ unless `--out` says otherwise, and `ge lab publish` refuses it without `--private-ok`: pak-derived pictures then never sit where a commit or a shared page could pick them up (weapons thread)
- a shared CPU scene view in engine/lab (a camera, a lit raster with occlusion and a tone map over meshes) for labs that draw a model or a room without the GPU: brush detail's detail_view.cpp is the third lab-local renderer of its kind
- a `ge` older than the base saying so (the commands main has that this build lacks, and `git merge origin/main` as the next step): a branch cut before `ge goals free` landed scanned branches by hand
- `ge build` and `ge lab` warning when the labs binary is older than a library it links
- `ge deps fetch`
- `ge lint --explain`
- the software Vulkan driver's leaks suppressed under `ge leaks`
- GPU labs on no adapter reported as skipped
- `ge add lab --kind=overlay`
- `ge services check` (a game's store catalog checked by `check_catalog`, and later diffed against a store's exported partner configuration)
- `ge doctor [--tools]` / `ge env` (what the machine lacks per ge command: toolchain versions, GPU adapter, display, Blender; each gap with its fix named, tools/setup-ubuntu.sh on Ubuntu)
- `ge shader check <file.wgsl>` (one shader's includes resolved as the embedder does, compiled by Tint, the error named with its line: `make shaders-check` builds every shader and needs GE_GPU, and a reserved word showed only as finish() returning false; and the same Tint error printed wherever a test or a lab sees `pipelines.finish` fail)
- `ge template check` (every `ge add` template and the scaffold filled at a short and a long game name, then `ge check` and clang-format run on each: a placeholder's tag line reflowed by clang-format broke only at a long name)
- the WGSL embedder failing on an empty generated shader (an unbalanced bracket in a comment splits the CMake list and silently empties it)
- `ge lab run --no-gpu` (a GPU lab forced onto its placeholder path, to check its digest matches a runner without an adapter, without a temporary code switch; #228)
- `ge lab new <id> --like=<lab>` (a lab's skeleton copied from another: the spec, a WAV loader, the sound shot; #274)
- `ge lab run <lab> --dprint` (a QC mod's dprint console lines shown live while a lab runs, in place of a throwaway test that prints a match report's console: M18d)
- `ge lab run <lab> --pak=<file>` remembered (an installed pack's path kept for content-dependent labs; #278)
- `ge fetch <url> --sha256=<hex> --into=<cache>` (one shared, verified download for the tools that fetch inputs at bake time, in place of each shelling out to curl; #286)
- `ge mod run <pkg> --lab=mod` (a mod built, loaded and run against a recorded match in one command; #289)

(`ge shot` is M12b's; `ge anim` is M14b's.)

## Shooter's Edge, the FPS port (M18: the ge tooling it needs)

- `ge pak ls|extract|build|convert|sum` (done: pak and pk3 listed with CRC-32 and SHA-256, files extracted, a directory packed, pak to pk3 and back, a file's sums kept and checked; `ge pak ls <gamedir> [--mods=a,b] [--hidden]` done too (each file's winning source and what it hides, over engine/vfs/search.hpp, .pk3dir included); next, a deflated .pk3 when the engine has a deflate writer; asked for by the classic-enhance and weapons threads and Niclas)
- `ge tool classicenhance -- extract <kind> <names>` (a pack's original textures or skins written at 1x as PNG to scratch, and the enhanced ones read back out of the cached glTF, in place of a throwaway pak, BSP and MDL reader per experiment)
- `ge bsp info|ents|shot` (lumps and limits, the entity lump as text, a top-down CPU picture of the leaves, the PVS from a point, the hulls)
- `ge progs build|disasm|run|trace|profile|diff` (the engine's own compiler for the game's VM language, and the `progs` VM inspected)
- `ge server --map --bots --for` (a headless dedicated server with bots, its input log written for replay, `ge state` and diverge)
- `ge demo info|frames|shot|diff` (the classic demo formats decoded, a frame pictured, two demos diffed to their first difference)
- `ge net inspect --protocol=<name>` (pluggable protocol decoders over a capture: `--protocol=classic`)
- `ge pmove <scenario>` (movement trajectories as tables or charts against a recording)
- `ge console <target> "<cmd>"` and `--pid=<p>` (console commands and cvars on a headless server or client, or a running game, the output read back)
- the play contract's gaps in `engine/play`: an angle axis kind on a finer grid (16-bit angles), a step whose length the input sets (a usercmd's msec), several players a Run

## Networking (M17)

- `ge net portmap open|renew|close` (a real port mapping from the command line, the OS's gateway and LAN address found: a real-router check on the Mac in one line)
- `ge net reach --host --client --preset --forward` (one pair of peers' reach timeline)
- `ge net token --master-env=NAME --host=<id> --client=<id>` (mint a connect token with the master key read from the named environment variable, and inspect one: test a real host by hand without a matchmaker)
- `net-link --attack=<name>` (run one of the lab's attacks on its own: isolating one took a temporary code edit)
- `ge net lobby --clients=N --script=<file>` (a session script, join, chat, kick, cut and restore, run against the simulated network, each end's event log and each client's player list diffed against the host's; #83)

## Shooter's Edge (M18, docs/shooters-edge.md § The `ge` tooling it needs)

- `ge pmove <scenario> --line=N` (one recorded movement scenario stepped against `engine/move`, stopping at the first line that differs, with both sides' state there; classic game loop thread)
- `ge bsp info/ents/shot` (a map's lumps and limits, its entity lump, a CPU picture of leaves, the PVS from a point and the hulls)
- `ge qc build/disasm/run/trace/profile/diff/assemble/builtins` (our QC compiler; a progs disassembled, run, traced a step at a time, timed by function, diffed against another build by the functions and fields that changed, and assembled from a text listing; `ge qc builtins <progs>`: the builtin numbers a progs calls, checked against the game's catalogue, so "will this mod run here" is one command)
- `ge server --map --bots --for` (a headless dedicated server with bots, writing an input log for `ge replay`, `ge state` and `ge diverge`; run, replayed and diffed from a game repo as well, which the lobby and hosting thread asked for)
- `ge demo info/frames/shot/diff` (a client or MVD demo decoded frame by frame, a frame pictured, two demos to their first difference)
- `ge net inspect --protocol=<name>` (pluggable decoders, so a capture of the classic protocol reads as its messages)
- `ge pmove <scenario>` (a movement scenario's trajectory as a table and a chart, against its parity recording)
- `ge console <target> "<command>"` (a console command or cvar run in a headless server or client, its output returned)
- `ge play` for first-person games (an angle axis on the protocol's grid, a step whose length the input sets, several players a run)
- `ge bot nav <map>` (build a map's navigation graph without a lab run, and print its node and link counts, the nodes no route reaches, and the proof of one route: bots thread, M18r; also `--near=x,y,z`, the nodes and links near a point: #293)
- `ge bot run <map> --from=<x,y,z> --to=<x,y,z> --hands=human --seeds=N` (one route ridden by the bot and printed as its trail, its mode timeline and its why-record, without editing a test: #293, M18r2)
- `--game=<zip>` on the labs that read a game folder (`ge lab run se-client --game=<release zip>`: the zip mounted as the game directory through `engine/vfs`, its packs read inside it, so pak0 is never extracted into an `id1` folder first; classic HUD thread)
- `ge lab <lab> --stage-pak=<fixture>` (a pack staged from a fixture of stand-in player models and the other models a demo draws, so the demo and picture labs draw players without the classic pak: today `se-highlights`' moment shots show empty halls; demo highlights thread; done: the runner's `--stage-pak=<name>`, `Lab::stage_pak()`, Shooter's Edge's `fixture` in client/stage.hpp, read by se-highlights, se-demo-video, se-timedemo and se-client)

## Store services

- `ge services check` (a game's achievements and stats catalog checked: ids unique, every stat an achievement reads defined; later diffed against a store's exported partner config)

## Input devices

- `ge input replay <events-log> --bindings=<file>` (each action's value frame by frame, from a recorded event log under a binding set)
- `ge play --events=<session>` and `ge replay <session>` (a recorded device session played headless through the game's bindings into a run: the step's inputs and the run's digest, where `ge input replay` stops at the actions; `ge replay` today takes only an input log)
- `ge input bindings check <file>` (a bindings file parsed against the game's actions: conflicts and unbound actions listed)
- `ge input devices` (what is plugged in: id, kind, key, features and the database row it matched, in place of a throwaway lab run; #130)

## Free content

- `ge content scan <pak|dir>` (done: every sound, model and numbered set a set ships or its maps name, held to the asset map and the registry, with an assetmap draft; next, what the game logic plays or shows, on the progs reader)
- `ge source search|fetch <source>:<pack>` (done: Kenney, OpenGameArt and Poly Haven found and unpacked into a cache with a SOURCE.txt, a licence the repo cannot carry refused; next, the hash written to a lock file)
- `ge sound match <reference> <candidates>` (done: ranked by length, bands and envelope with the distance per trait; next, a sheet of waveforms)
- `ge sound import` and `ge model import` (done: converted to the loudness and format, textures packed, load-checked; next, a preview and the registry and credits rows written)
