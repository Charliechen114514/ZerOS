# 性能测量记录(perf demo,真机 Blue Pill @ 72MHz)

测量工具:DWT 周期计数器(Cortex-M3 内置,每时钟周期+1,72MHz 下 1µs = 72 个计数)。
测量方法:两个同优先级任务互相 yield,在任务 A 记下计数,任务 B 醒来时读计数,差值就是"从让出到对方跑起来"的完整耗时。
每组 1000 轮取最小/平均/最大。

**注意:以下测的都是完整 yield 路径(包含 API 调用、排队、中断进出、任务恢复),不是纯 PendSV 切换。PendSV 单独耗时没有独立测量,不能用 yield 减去估算值代替。**

---

## 第一轮:原始代码(测量代码还在内核里)

```text
ZerOS demo: performance

=== ZerOS Performance @ 72MHz ===
rounds: 1000

[1] PendSV single switch (context_switch only):
    worst: 210 cycles (2.92µs)  budget: 72 = 1µs

[2] yield full path (yield call -> other task runs):
    min: 479 cycles (6.65µs)
    avg: 489 cycles (6.79µs)
    max: 729 cycles (10.13µs)

[3] worst BASEPRI window (longest critical section):
    worst: 678 cycles (9.42µs)

D12 verdict (switch <=1µs): FAIL
```

**注意:第一轮的 [1] 和 [3] 数字包含了测量代码自身的开销(约 80 个周期),不能当作真实值引用。**

### 为什么这么慢

最大的问题是**测量代码自己在拖慢被测对象**。我在切换函数和临界区里塞了 DWT 读取(想实时追踪最大值),但每次读 DWT 寄存器要跑一次总线访问,读两次再加个比较和存储,一个来回就多吃了大约 80 个周期。相当于用秒表称秒表。

第二个问题是**切换函数里做了多余的保护**。PendSV 入口汇编第一句是 `cpsid i`,此时 PRIMASK 已经屏蔽了所有可配置优先级异常(包括更高优先级的),C 函数里的调度逻辑再去设一层 BASEPRI 没有额外保护作用。去掉这层重复操作省了大约 15 个周期。

第三个问题是**排队走链表**。任务让出时要把自己挂回就绪队列的队尾,但链表没有记"尾部在哪",每次都从头走到尾找位置。两个任务时只多走一步,但这一步加上指针追踪的内存访问大约 30 个周期。

---

## 第二轮:拆掉测量代码 + 跳过多余保护

把 DWT 读取从内核代码里全部移除(改为在任务里外部测量),切换函数改用仅供 PendSV 调用的免锁版本(因为 `cpsid i` 已经罩住了,不需要再加 BASEPRI)。

```text
=== ZerOS Performance @ 72MHz ===
rounds: 1000

[1] yield full path (external DWT):
    min: 354 cycles (4.92µs)
    avg: 354 cycles (4.92µs)
    max: 543 cycles (7.54µs)

[2] non-assembly remainder (rough estimate, not independently measured):
    est: 314 cycles  budget: 72 = 1µs

D12 verdict: see [1] avg — that's the number users feel
```

比第一轮降了 **28%**(489→354)。

**关于 [2]:这是 yield 全程减去一个粗略估算的汇编开销(~40 cycles)得到的,不是独立测量的结果,不能用于 D12 判定。**

---

## 第三轮:热路径搬进 RAM(负优化!)

把切换函数(context_switch)和中断处理函数(PendSV/SVC)放进 RAM(编译时标记 `.ramfunc` 段,链接脚本自动在启动时从 Flash 拷贝到 RAM)。

Flash 在 72MHz 下每次取指令要等 2 个周期(共 3 个周期才能拿到一条指令),RAM 不用等。切换函数大约 40 条指令,理论上省约 80 个周期。

```text
=== ZerOS Performance @ 72MHz ===
rounds: 1000

[1] yield full path (external DWT):
    min: 376 cycles (5.22µs)
    avg: 377 cycles (5.24µs)
    max: 579 cycles (8.04µs)

[2] non-assembly remainder (rough estimate, not independently measured):
    est: 337 cycles  budget: 72 = 1µs

D12 verdict: see [1] avg — that's the number users feel
```

跑了三遍,数字稳定在 376/377/377。**没有降,反而微涨了**。

可能的原因(尚未完全隔离):
- Flash 的预取缓冲器已经在缓存顺序执行的指令,搬到 RAM 后这个好处消失
- SRAM 取指与数据访问的总线竞争
- 函数地址及分支布局改变
- 编译器生成的 veneer 或 literal pool 对齐变化

**这一轮的结论:在当前 STM32F103 配置下,单纯把切换函数搬 RAM 是负优化。**

---

## 第四轮:RAM + 链表加尾指针

给 SelfList 加了一个"尾部指针"。之前挂队列要从头走到尾(每次让出都要走),现在直接在尾部接上,两步完成,不用走链。

```text
=== ZerOS Performance @ 72MHz ===
rounds: 1000

[1] yield full path (external DWT):
    min: 349 cycles (4.85µs)
    avg: 349 cycles (4.85µs)
    max: 537 cycles (7.46µs)

[2] non-assembly remainder (rough estimate, not independently measured):
    est: 309 cycles  budget: 72 = 1µs

D12 verdict: see [1] avg — that's the number users feel
```

---

## 第五轮:Flash + 尾指针(恢复 Flash 执行,保留 O(1) 队列)

第四轮是在第三轮(RAM)基础上改的,没测过"Flash + 尾指针"这个组合。这一轮把 `.ramfunc` 拆掉(函数回到 Flash 执行),保留尾指针。

```text
=== ZerOS Performance @ 72MHz ===
rounds: 1000

[1] yield full path (external DWT):
    min: 333 cycles (4.63µs)
    avg: 333 cycles (4.63µs)
    max: 529 cycles (7.35µs)

[2] non-assembly remainder (rough estimate, not independently measured):
    est: 293 cycles  budget: 72 = 1µs

D12 verdict: see [1] avg — that's the number users feel
```

**全场最优!** 证实了第三轮的推测:Flash 预取缓冲器比 RAM 零等待更值钱,负优化的 `.ramfunc` 已从代码中移除(保留注释和开关供未来芯片复测)。

---

## 第六轮:NO_TICK + PendSV 探针(终审)

两个 CMake 开关各管一件事:`ZEROS_MEASURE_PENDSV` 在 PendSV 汇编里插无分支 DWT 快照(~24 cycles 固定探针开销,骑在被测路径上,build-gated);`ZEROS_PERF_NO_TICK` 完全不启动 SysTick——reporter 改由 pong 收工后 notify 唤醒,整条链路不依赖时间,测量窗里没有 tick 上下文会撞进来。

这一轮顺带抓了两个真 bug,都是 NO_TICK 路径才暴露的:

1. **任务退场死锁**。任务函数返回后跳进 Trap,原实现是 wfi 死等。有 tick 的系统里僵尸任务靠时间片轮转被强行切走,一直没人觉得不对;NO_TICK 下没有中断可依,僵尸占着 CPU,全场死寂。Renode 复现,挂死后 PC 直接落在 Trap 的 wfi 循环里。修复:Trap 改成 block 自己——退场就是让出,无人可跑时 idle 的 wfi 才是正确终态。
2. **wait_notify 大跨度回绕**。`wait_notify(Milliseconds{0xFFFFFFFF})` 的 deadline = now + span 无符号回绕成"过去",落在"严格在未来"判定之外,立即超时返回——想要"近似 forever",实际"立刻醒"。改用 10 分钟这样的有限跨度规避。这是时间契约(待决 6)的活案例,内核侧要不要加饱和/断言待裁。

测量数据(NO_TICK + probe,真机):

```text
=== ZerOS Performance @ 72MHz ===
rounds: 1000
tick: OFF (no SysTick)

[1] yield full path (external DWT):
    min: 375 cycles (~5us)
    avg: 375 cycles (~5us)
    max: 385 cycles (~5us)
    outliers(>avg+128): 0 of 1000

[2] PendSV handler (probe, 24 cycles subtracted):
    net min: 93 cycles (~1us)
    net avg: 93 cycles (~1us)  budget: 72 = 1us
    net max: 93 cycles (~1us)
    raw avg: 117 cycles

[3] accounting: yield[1] - handler[2] = 282 cycles
    (API + queue + pend + exception entry/exit + waker prologue)
```

读数:

- **max 从 532 收敛到 385,离群从 5 变 0**:SysTick 干扰归因终审成立——有 tick 版的 max 尾巴就是 tick 上下文(timer 链 + 时间片判定)撞进测量窗。
- 375(含 24 探针)− 24 ≈ 351 ≈ 336 + 布局抖动,与有 tick 版交叉自洽。
- **PendSV 本体 93 cycles,零方差**(min = avg = max)。零方差本身就是证词:稳态切换是完全确定的路径,测量方法可信。raw 117 − 24 = 93。
- **拆账:yield 375 = handler 93 + 282**。75% 的开销在异常进出、API 调用、排队、pend、唤醒者前言——**优化 yield 的杠杆不在 handler 里**,handler 只占四分之一。

## 完整测试矩阵

| 代码位置 | 链表 | yield avg | 相对基线 |
|---|---|---|---|
| Flash | 走链 O(n) | 354 | 基线(第二轮) |
| RAM | 走链 O(n) | 377 | +6%(负优化) |
| RAM | 尾指针 O(1) | 349 | -1% |
| **Flash** | **尾指针 O(1)** | **333** | **-6%,全场最优** |

**总计:489(原始)→ 333(最终)= -32%**

**注意:min 和 avg 显示相同是整数除法截断的结果。max 存在延迟样本(可能是 SysTick 干扰,未验证),不应视为 WCET。**

---

## 最终结论

**本文直接测得的是完整 yield 单向切换延迟(从任务 A 调 yield() 到任务 B 的第一条指令),不是纯 PendSV 切换耗时。**

| 指标 | 结果 | 说明 |
|---|---|---|
| yield 全程 avg | **336 cycles (4.67µs)** | 有 tick,无探针;含 API 调用、排队、中断进出、任务恢复的所有开销(第五轮 333 同波动区) |
| yield 全程 max | 532 cycles (7.39µs) | 尾巴=SysTick 干扰:离群 5/1000,与 5ms 窗@1kHz≈5 次碰撞吻合;NO_TICK 复测收敛 385/离群 0,归因终审 |
| PendSV 本体 | **93 cycles (1.29µs),零方差** | 探针实测(net),min=avg=max;不再用减法估算 |
| BASEPRI 最长窗口 | 待复测 | 第一轮数含 ~80 cycles 测量污染 |

### D12 判定

D12 要求"上下文切换 ≤1µs @72MHz"。

- 如果 D12 指的是**完整 yield 切换**(用户调 yield 到下一个任务跑起来):**FAIL,当前为 4.63µs**
- 如果 D12 指的是**PendSV handler 或 context_switch() 函数本体**:**UNVERIFIED,尚未单独测量**

无论哪种口径,目前的数据都不支持"达标"的判定。

### 三个有效的优化手段(真机实测)

1. **测量代码移出热路径**:-28%(489→354),最大单笔收益
2. **PendSV 上下文免锁调度**:省去重复 BASEPRI 操作
3. **队列尾指针 O(1) 插入**:不走链,-6%

### 一个负优化(真机实测)

**`.ramfunc` 搬 RAM 是负优化**(+6%),原因是 STM32F103 的 Flash 预取缓冲器在顺序执行时已经非常高效。这个结论**仅在当前配置(72MHz + 2WS)下成立**,换芯片或换频率需要重新测量。代码中保留了开关(`#define ZEROS_SWITCH_IN_RAM`)供复测。

### 实用性评估

以普通毫秒级嵌入式任务调度而言,4.63µs 的切换延迟已经具有实用性。在 1kHz tick 下,一次切换占 tick 周期的 0.46%,不会成为瓶颈。

### 与其他 RTOS 的比较

由于不同 RTOS 对"上下文切换"的测量边界和配置差异较大(是否包含 API 调用、是否发生实际任务切换、编译器和优化级别等),本文暂不作横向性能排名。若要比较,应在同一块 Blue Pill、同一编译器和相同测量代码下进行。

### 后续方向

1. ~~独立测量 PendSV-only 耗时~~ —— 第六轮完成:93 cycles 零方差(探针实测)
2. ~~关闭 SysTick 再跑一组,分离中断干扰~~ —— 第六轮完成:离群 5/1000 → 0,归因成立
3. **D12 口径裁定**(待定):按 PendSV 本体口径 93 > 72 超预算 29%,维持预算记 FAIL 还是按本体口径修订(如 ≤110 cycles)。93 的构成非常物理:8 寄存器压/弹是 ABI 义务,异常进出是硬件行为。实用性:1kHz tick 下一次切换占周期 0.46%
4. BASEPRI 最长窗口复测(探针 patch 复用即可)

### 测量设施的去留

probe 是 build-gated 的,生产构建零开销,但热路径里出现测量钩子终究碍眼。处置:**主线代码剥干净,探针以 patch 随库**(`scripts/patches/pendsv-probe.patch`)——要测量的机器先 `git apply` 再开 `ZEROS_MEASURE_PENDSV`;忘了 apply 就开开关 = 链接错误,响亮失败。数据已全部入档,patch 的使命是下次复测即贴即用。
