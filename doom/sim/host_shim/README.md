# host_shim

<!-- SPDX-License-Identifier: BSD-3-Clause -->

pthreads-based host-side shim for the fc-pico Doom port's `host` build (`PICO_PLATFORM=host`),
filling in exactly what pico-sdk 2.1.1's host platform ships headers for but no implementation
of, without pulling in SDL2. This is the piece `doom/plan/08-build.md`'s "host build" paragraph
calls for and its P0-T3 asks to confirm by linking.

## What the SDK host platform provides vs what this shim adds

pico-sdk's host platform (`src/host/`) is deliberately minimal --- its own README
(`src/host/README.md`) says so directly: it is "a minimal environment to compile programs...
sufficient for programs that don't access hardware directly," and multicore, alarms and
audio/scanvideo come from a separate project, [pico-host-sdl](https://github.com/raspberrypi/pico-host-sdl),
which needs SDL2. The table below is the result of actually reading the files named, not just
the README's summary of them.

| Area | What the SDK host platform provides | What was missing | What this shim adds |
|---|---|---|---|
| Threads / core 1 | `get_core_num()` (`src/host/pico_platform/platform_base.c`) -- a `PICO_WEAK_FUNCTION_DEF` stub that always returns 0 | Everything in `pico/multicore.h` (`src/host/pico_multicore/include/pico/multicore.h`): the header ships, but `src/host/pico_multicore/CMakeLists.txt` adds no `.c` file to the `pico_multicore` target at all -- it is headers only. Confirmed empirically (see "Q3" below): all 17 declared functions are undefined at link time. | `multicore_host.c`: `multicore_launch_core1()` (and the `_with_stack`/`_raw` variants, which delegate to it) as a detached pthread; the inter-core FIFO as two independent 8-entry mutex+condvar queues (see "FIFO semantics" below); `multicore_lockout_*` as no-ops; `multicore_reset_core1()` as a best-effort `pthread_cancel`+`join`. Also overrides `get_core_num()` -- see "get_core_num() caveat" below. |
| Time: `time_us_64`, `busy_wait_us` | Yes. `src/host/hardware_timer/timer.c`'s `time_us_64()` reads `clock_gettime(CLOCK_MONOTONIC, ...)`; `busy_wait_us()`/`busy_wait_until()` work the same way. | -- | Not touched. |
| Time: `sleep_ms`, `sleep_us` | Yes, but not via a low-power alarm-driven wait: `src/host/hardware_timer/CMakeLists.txt` sets `PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1` for the host, which routes `src/common/pico_time/time.c`'s `sleep_us()` through its `#else` branch -- `busy_wait_until()`, which on `__unix__` is a real `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)`, not a spin loop. | -- | Not touched. |
| `add_alarm_in_us`, `add_alarm_in_ms`, `cancel_alarm`, `add_repeating_timer_us`, `add_repeating_timer_ms` | No. `src/common/pico_time/include/pico/time.h` guards all five behind `#if !PICO_TIME_DEFAULT_ALARM_POOL_DISABLED`, which the host sets to 1 (see above). They are not merely unimplemented -- they are not *declared*, so calling one is an implicit-declaration warning (an error under `-Werror`) and then an undefined reference. Confirmed empirically below. | `add_alarm_in_us`, `cancel_alarm`, `add_repeating_timer_ms` and `add_repeating_timer_us` | `alarm_host.c` + `alarm_host.h`: an independent scheduler backed by one background pthread (see "Why not reuse `alarm_pool_t`" below). `add_alarm_in_ms()` is not reproduced -- it is a two-line wrapper around `add_alarm_in_us()` in the original SDK, and callers can write the same wrapper themselves. |
| `cancel_repeating_timer` | Declared *unconditionally* in `pico/time.h` and *defined* unconditionally in `src/common/pico_time/time.c`, so it links fine even on host. | Not missing as a symbol -- but see "The `cancel_repeating_timer` trap" below: its only successful path panics on host. | `host_cancel_repeating_timer()` in `alarm_host.h`/`alarm_host.c` -- use this instead for timers created by this shim's `add_repeating_timer_us`/`_ms`. |
| `semaphore_t` (`pico/sem.h`) | Yes -- common code (`src/common/pico_sync/sem.c`), platform-independent. Builds and links on host with no changes. | -- | Not touched -- but see "hardware_sync's dummy spin lock" below for a caveat about using it across the pthreads this shim creates. |
| Spin locks / `hardware_sync` | Yes, in the sense that it builds and every function returns something (`src/host/hardware_sync/sync_core0_only.c`). See the caveat below for what that implementation actually does. | -- | Not touched -- flagged as a limitation instead; see below. |
| `hardware_gpio`, `hardware_irq`, `pico_stdlib`, `pico_time` (aggregation) | Yes, all build cleanly for `PICO_PLATFORM=host` with gcc 13 / pico-sdk 2.1.1; no workarounds were needed. | -- | Not touched. |

### Q3: the exact list of SDK host functions missing at link time before this shim

Confirmed by building a throwaway probe (`project(...); pico_sdk_init(); add_executable(...)`
linking `pico_stdlib pico_multicore pico_sync pico_time` for `PICO_PLATFORM=host`, gcc 13,
pico-sdk 2.1.1) that calls every function `pico/multicore.h` declares, then reading the
linker's undefined-reference list -- all 17 of them, and *only* these (no other host-platform
undefined references, e.g. no time/sem/gpio symbols missing):

```
multicore_launch_core1
multicore_launch_core1_with_stack
multicore_launch_core1_raw
multicore_reset_core1
multicore_fifo_rvalid
multicore_fifo_wready
multicore_fifo_push_blocking
multicore_fifo_push_timeout_us
multicore_fifo_pop_blocking
multicore_fifo_pop_timeout_us
multicore_fifo_drain
multicore_fifo_clear_irq
multicore_fifo_get_status
multicore_lockout_victim_init
multicore_lockout_start_timeout_us
multicore_lockout_start_blocking
multicore_lockout_end_timeout_us
multicore_lockout_end_blocking
```

Separately, `add_alarm_in_us`, `cancel_alarm` and `add_repeating_timer_ms` are not link
failures at all -- they fail to *compile* (`warning: implicit declaration of function ...`,
an error under this project's `-Werror`) before ever reaching the linker, because
`PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1` on host removes their declarations from
`pico/time.h` entirely (see the table above). `time_us_64`, `sleep_ms`, `sleep_us`,
`busy_wait_us` and `sem_*` all compiled and linked with no changes needed.

## Why not reuse `alarm_pool_t` for the missing alarm functions

`src/common/pico_time/time.c`'s alarm-pool machinery is built to be driven by a real hardware
timer IRQ, through hooks in `src/host/pico_time_adapter/time_adapter.c`
(`ta_set_timeout`, `ta_enable_irq_handler`, `ta_force_irq`, `ta_hardware_alarm_claim`, ...).
Every one of those hooks is stubbed to `panic_unsupported()` on host. So creating even an empty
`alarm_pool_t` (`alarm_pool_create_on_timer()` calls `ta_hardware_alarm_claim()` immediately)
already panics -- there is no way to make the real alarm pool work on host without editing the
SDK, which is out of scope. `alarm_host.c` is therefore an independent scheduler: one background
pthread sleeping on a `CLOCK_MONOTONIC` condvar (matching `time_us_64()`, which also reads
`CLOCK_MONOTONIC`) until the next due alarm or repeating timer, running callbacks outside its
lock so they can themselves call `add_alarm_in_us()`/`cancel_alarm()` without deadlocking.

### The `cancel_repeating_timer` trap

`pico/time.h` declares `cancel_repeating_timer()` unconditionally (unlike the other four), and
`src/common/pico_time/time.c` defines it unconditionally too, so linking against it is not an
error. But trace what it does: `cancel_repeating_timer()` &rarr; `alarm_pool_cancel_alarm()`,
and on a successful match that function unconditionally calls `ta_force_irq(pool->timer, ...)`
-- which, as above, panics the whole process on host. So this symbol is worse than a link
error: it builds fine and only blows up at runtime, and only when it would otherwise have
succeeded. Because it is a normal (non-weak) symbol already linked in from `pico_time`, this
shim cannot shadow or redefine it. `add_repeating_timer_us()`/`_ms()` here fill in
`repeating_timer_t.pool` with `NULL` (not a real `alarm_pool_t`) precisely so nothing
accidentally reaches that code path, and timers created through them must be cancelled with
this shim's own `host_cancel_repeating_timer()` (declared in `alarm_host.h`) instead of the
SDK's `cancel_repeating_timer()`.

## FIFO semantics

`multicore_host.c` models the SIO inter-core FIFO as **two** independent 8-entry mutex+condvar
queues, not one shared queue: on real hardware each core's push goes to its own write FIFO and
each core's pop reads the *other* core's write FIFO, so "push" and "pop" resolve to different
physical queues depending on which core calls them. This shim keys that routing off
`get_core_num()`. Both push and pop are hardware-faithful blocking operations: a push blocks
while its queue holds 8 entries, a pop blocks while its queue is empty. `multicore_fifo_get_status()`
reports the two bits the task asked for (VLD = bit 0, RDY = bit 1); overflow/underflow (WOF/ROE
on real hardware) are not modelled since blocking push/pop makes them unreachable here, so
`multicore_fifo_clear_irq()` is a faithful no-op.

One consequence worth calling out because it is easy to trip over (the test in this directory
did, during development): with real blocking semantics, a protocol that pushes everything
before popping anything **will deadlock** exactly as it would on real hardware once either
side's queue fills and the other side stops draining. `test_host_shim.c`'s FIFO exchange
interleaves push and pop per element for this reason.

## `get_core_num()` caveat and the override this shim adds

`src/host/pico_platform/platform_base.c` defines `get_core_num()` through the
`PICO_WEAK_FUNCTION_DEF`/`PICO_WEAK_FUNCTION_IMPL_NAME` pair, which on GCC/Clang expands to
`#pragma weak get_core_num` immediately before `uint get_core_num() { return 0; }`. Two things
follow from reading that:

1. Without this shim, `get_core_num()` always returns 0 on host, on every thread. Code running
   on a pthread-backed "core 1" would see `get_core_num() == 0`, indistinguishable from core 0.
   Anything gated on `get_core_num() == 1` -- including the engine's
   `disallow_core1_malloc`-style assertions -- can never fire under a naive host build, because
   the condition they check for is unreachable.
2. `#pragma weak` is not incidental; it is exactly the hook the task description asked to look
   for: it marks the SDK's own definition weak *so that a strong definition elsewhere in the
   final link can replace it*. This was verified empirically (a throwaway strong
   `get_core_num()` in another translation unit, linked alongside `pico_stdlib`, wins over the
   SDK's weak one).

Because that hook exists, `multicore_host.c` provides a strong `get_core_num()` backed by a
`__thread` variable that the core 1 trampoline sets to 1 before calling the launched entry
point. Under this shim, `get_core_num()` therefore correctly reports 0 on the launching thread
and 1 on the launched one, and anything (engine code included) that branches on it behaves the
way it would on real hardware.

## hardware_sync's "dummy" spin lock

Worth documenting because it is not obvious from the header alone and it directly affects how
safe it is to share pico_sync primitives (`semaphore_t`, `mutex_t`, `critical_section_t`)
across the real OS threads this shim creates. `src/host/hardware_sync/sync_core0_only.c` says
outright in a comment: "This is a dummy implementation that is single threaded." Concretely,
`spin_lock_unsafe_blocking()` is just:

```c
void spin_lock_unsafe_blocking(spin_lock_t *lock) {
    lock->locked = true;
}
```

It does not check `lock->locked` first, spin, or use an atomic exchange -- it is an
unconditional plain write, with no memory fence anywhere in the file either. On real hardware
the RP2040/RP2350 spin lock registers provide actual mutual exclusion; this host stand-in
provides none. It is adequate for pico-host-sdl's model (cooperative, effectively
single-threaded), but it means every pico_sync primitive that is built on `hardware_sync` --
which includes `semaphore_t`, since `sem.c`'s critical sections are `spin_lock_blocking()` /
`spin_unlock()` around a handful of instructions -- has a real, if very narrow, data-race
window when two genuine pthreads (core 0 and this shim's core 1) touch the same primitive at
truly the same instant. `__wfe()`'s backing flag (`event_fired`) compounds this: it is a single
process-wide bool that `__sev()` sets and nothing ever clears, so after the first
`__sev()` anywhere in the process every subsequent `__wfe()` anywhere returns immediately; this
does not break correctness (`sem_acquire_blocking()`'s enclosing loop re-checks the actual
permit count each time), but it does turn `sem_acquire_blocking()`/`sem_acquire_timeout_ms()`
into a real busy-spin on host rather than a real sleep once anything else in the process has
called `__sev()`.

None of this is fixable without editing the SDK, which is out of scope here, so it is not
patched. `multicore_host.c`'s own FIFO uses its own pthread mutex/condvar throughout for
exactly this reason -- it does not build on `hardware_sync` at all. `test_host_shim.c` still
exercises `semaphore_t` across the two threads as asked, sequenced with a generous sleep before
the cross-thread release so the two sides' critical sections don't land back-to-back, but this
is a real (if narrow) limitation of the pico-sdk host platform worth keeping in mind for any
other cross-thread use of pico_sync primitives in the host build.

## Build and test

```sh
cmake -S /home/user/fc-pico/doom/sim/host_shim -B /tmp/build-shim -G Ninja \
    -DPICO_SDK_PATH=/home/user/pico-sdk -DPICO_PLATFORM=host
cmake --build /tmp/build-shim
ctest --test-dir /tmp/build-shim --output-on-failure
```

Observed output (gcc 13.3.0, cmake 3.28.3, ninja 1.11.1, pico-sdk 2.1.1, 4 cores):

```
1/1 Test #1: host_shim_test ...................   Passed    0.21 sec

100% tests passed, 0 tests failed out of 1
```

The host platform itself (`pico_stdlib`, `pico_multicore`, `pico_sync`, `pico_time`) built
cleanly with no errors or workarounds needed; `-Wall -Wextra -Werror` is scoped (via
`set_source_files_properties()` in `CMakeLists.txt`) to exactly this directory's three `.c`
files, not to the pico-sdk sources that `pico_stdlib` and friends inject into any target that
links them, some of which do warn (e.g. `src/host/pico_stdlib/stdlib.c`'s unused `freq_khz`
parameter) and would break a target-wide `-Werror`.

## Files

| File | Purpose |
|---|---|
| `CMakeLists.txt` | Standalone project: imports pico-sdk, builds the `host_shim` static library and the `host_shim_test` executable (registered with `add_test`). |
| `pico_sdk_import.cmake` | Unmodified copy of `pico-sdk/external/pico_sdk_import.cmake`. |
| `multicore_host.c` | `pico/multicore.h` implementation (pthreads) + the `get_core_num()` override. |
| `alarm_host.c` / `alarm_host.h` | `add_alarm_in_us`, `cancel_alarm`, `add_repeating_timer_us`/`_ms`, `host_cancel_repeating_timer` (background-pthread scheduler). |
| `test_host_shim.c` | Launches core 1; a 1000-word-each-way FIFO round trip with checksums; a single-threaded then cross-thread `semaphore_t` exercise; `time_us_64()`/`sleep_ms()`; a repeating-timer fire-count check. Nonzero exit on any failure. |
