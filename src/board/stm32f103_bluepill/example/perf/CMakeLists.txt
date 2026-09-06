zeros_add_demo(perf)

# Measurement build gates (both default OFF, keeping this demo a plain CI
# citizen; configure a separate build dir to turn them on, see
# documents/notes/perf_1.md):
#   ZEROS_MEASURE_PENDSV — probe the PendSV handler; REQUIRES
#                          scripts/patches/pendsv-probe.patch applied first
#                          (mainline keeps the hot path clean; skipping the
#                          patch fails to build — loudly, on purpose)
#   ZEROS_PERF_NO_TICK   — start no SysTick at all; the reporter wakes on
#                          notify, so nothing depends on time
option(ZEROS_MEASURE_PENDSV "perf: probe the PendSV handler with DWT stamps" OFF)
option(ZEROS_PERF_NO_TICK "perf: run without SysTick to isolate tick interference" OFF)
if(ZEROS_MEASURE_PENDSV)
    target_compile_definitions(zeros-demo-perf PRIVATE ZEROS_MEASURE_PENDSV)
endif()
if(ZEROS_PERF_NO_TICK)
    target_compile_definitions(zeros-demo-perf PRIVATE ZEROS_PERF_NO_TICK)
endif()
