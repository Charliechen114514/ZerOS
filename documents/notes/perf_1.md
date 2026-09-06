
第一次perf
```text
=== ZerOS Performance @ 72MHz ===
rounds: 1000

[1] PendSV single switch (context_switch only):
    worst: 210 cycles (~2us)  budget: 72 = 1us

[2] yield full path (yield call -> other task runs):
    min: 479 cycles
    avg: 489 cycles
    max: 729 cycles

[3] worst BASEPRI window (longest critical section):
    worst: 678 cycles

D12 verdict (switch <=1us): FAIL

```

登记一下原因：



第二次perf:
```text
ZerOS demo: performance

=== ZerOS Performance @ 72MHz ===
rounds: 1000

[1] yield full path (external DWT):
    min: 354 cycles (~4us)
    avg: 354 cycles (~4us)
    max: 543 cycles (~7us)

[2] PendSV C-path estimate (yield - ~40 asm):
    est: 314 cycles (~4us)  budget: 72 = 1us

D12 verdict: see [1] avg — that's the number users feel

```

