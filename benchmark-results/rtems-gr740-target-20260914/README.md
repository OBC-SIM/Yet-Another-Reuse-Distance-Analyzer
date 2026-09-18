# GR740 target execution archive (2026-09-14 UTC)

This archive holds the first successful RTEMS 6 / GR740 target execution of the
sixteen `backend/experiments/rtems_gr740` images. The
[2026-09-14 host archive](../rtems-gr740-20260914/README.md) recorded target
execution as `unavailable`; nothing in that archive is modified here.

## What was run

Each image was built from the same sources and flags as the host evaluation
(`backend/experiments/rtems_gr740/Makefile`, `sparc-rtems6-gcc 13.3.0`, GR740
BSP at `/opt/rtems/6`, `-O0 -g -mcpu=leon3 -mfpu -mhard-float`) and executed on
a GR740 instruction-set simulator with a command-line console.

Each ELF was loaded as the core 0 RAM image and run to completion, and the
console's performance statistics were read afterwards. The data cache matrix
repeats that procedure with core 0's data cache turned off before the run; the
instruction cache stays enabled in both arms, so only the data cache varies.

No X display was required. The `Gtk-WARNING: cannot open display` failure
recorded in the host archive did not reproduce on this host.

## Files

| File | Contents |
| --- | --- |
| `inputs.json` | per case: kernel, compile defines, ELF sha256 and size |
| `run-summary.txt` | one line per case: exit code, completion marker, valid counts, cycles, instructions, CPI, simulated time |
| `dcache-matrix.csv` | 32 rows: each case with core 0 data cache enabled and disabled |
| `logs/<case>.run.log` | console transcript of the plain run |
| `logs/<case>.dcache-{enabled,disabled}.log` | transcripts of the cache matrix arms |

Only the four summary files are tracked by Git; `logs/` stays local.

`Init` prints `YARDA_RTEMS,job=<n>,elapsed_ns=<t>,valid=<0|1>` for one warm-up
(`job=-1`) and ten measured jobs, then `YARDA_RTEMS_COMPLETE` only after every
job's closed-form output check passes.

## Results

All sixteen images reached `YARDA_RTEMS_COMPLETE`; 176 of 176 jobs reported
`valid=1` and none reported `valid=0`. The data-cache matrix repeated both arms
for all sixteen cases, 32 runs, with the same completion and validation result.

`kernel_median_ns` in `dcache-matrix.csv` is the median of the ten measured jobs
(the warm-up is excluded), taken from the guest's own
`rtems_clock_get_uptime_nanoseconds()` around `benchmark_kernel()`. It therefore
excludes RTEMS boot, initialization and output validation. `cycles`,
`instructions` and `cpi` come from the simulator console's performance
statistics and cover the whole program including boot.

## Scope

These are simulator observations, not GR740 silicon measurements, and not cache
counters: the simulator exposes no cache hit/miss statistics. Its performance
statistics report only cycles, instructions and CPI; the L4STAT statistics unit
is absent from the system device listing and reads as zero; the L2 cache control
register accepts the hit rate status mode bit but the status register never
changes; and the data cache dump command returns the same bytes as the
instruction cache dump. The data cache is modeled — disabling it changes cycles
and guest-measured kernel time — but only through its timing effect.

The disabled arm turns off the core 0 L1 data cache while the L2 stays enabled,
so the difference between arms measures the L1 data cache against the L2, not
against memory.
