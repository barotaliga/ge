// THE LAB RUNNER: the `labs` command line every registered lab is run through (`make lab`,
// `make labs`, CI's `labs_smoke`). It owns the flags every lab shares, the rule that an
// unknown flag is an error (a measurement that ignores a mistyped flag reports a confident
// wrong number), the output directory, the before-and-after
// comparison, the in-process determinism check and the committed digest baseline.
//
//   labs                                   the list: id, kind, owner, skill, question
//   labs --json                            the list as one JSON array (what `ge lab` reads)
//   labs <id> [--flag=value ...]           run one lab; its own flags are in `labs <id> --help`
//   labs <id> --tag=before                 write to previews/<id>@before/ instead
//   labs <id> --compare=before             print every metric beside the tagged run's
//   labs --all --smoke                     every lab at its smoke flags (CI)
//   labs --all --smoke --repeat=2          ... twice each; different digests fail the run
//   labs --all --smoke --check-digests=F   ... and each digest against the baseline file F
//   labs --all --smoke --shard=2/4         ... only every 4th lab from the 2nd (CTest's slices)
//   labs --all --smoke --write-digests=F   rewrite the baseline (a reviewed change)
//
// Exit 0 when every lab ran clean, 1 when a lab failed or a digest moved, 2 on a usage error.
//
// ge:summary The labs command line: list, run, tag, compare, smoke, repeat, digest baseline
// ge:keywords lab runner cli flags smoke compare tag repeat digests baseline previews shard
// ge:keywords usage help
// ge:skill labs
// ge:test tests/lab_runner_test.cpp
#pragma once

namespace ge::lab {

// The whole command line above. `main` in engine/lab/main.cpp is one call to it.
int run_labs(int argc, const char* const* argv);

}  // namespace ge::lab
