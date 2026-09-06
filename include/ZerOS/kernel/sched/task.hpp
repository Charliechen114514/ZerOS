#pragma once

// The task factory: the chain IS the contract. Skip a step and the rest
// of the chain simply does not exist — no comment can save you then.
//   named("blink").prio(1).stack(stack).entry(fn, arg).spawn_into(box)

#include "ZerOS/kernel/sched/stack.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/kernel/sched/types.hpp"

#include <concepts>
#include <cstdint>
#include <span>

namespace ZerOS::task {

using sched::TaskPriority_t;

// stages: each struct is what the chain knows so far, nothing more
struct Named {
    const char* name;
};
struct Priorized {
    const char* name;
    TaskPriority_t prio;
};
struct Stacked {
    const char* name;
    TaskPriority_t prio;
    std::span<std::uint32_t> stack;
};

// ONE declaration per task: the box (TCB storage) and the bed (stack)
// belong together — a Slot is both. The everyday form:
//     constinit task::TaskSlot<64> a{};
//     spawn(task::named("a").prio(1).stack(a).entry(fn, arg).spawn_into());
template <std::size_t Words = 64>
struct TaskSlot {
    sched::TCBStorage tcb{};
    TaskStack<Words> stack{};
};

// the slot-chain stages: the slot pointer rides along, spawn_into() has
// nothing to remember — the slot knows where everything lives
struct Slotted {
    const char* name;
    TaskPriority_t prio;
    sched::TCBStorage* box;
    std::span<std::uint32_t> stack;
};

// the size-declaring road: storage comes from the launch arena, so the
// chain only needs to SAY how many words — the arena does the laying-out
struct Worded {
    const char* name;
    TaskPriority_t prio;
    std::size_t words;
};

// fully armed for the arena road: config complete, launch() will do the rest
struct Launchable {
    const char* name;
    TaskPriority_t prio;
    std::size_t words;
    sched::UserTaskWrapper run;
};

// fully armed: everything said, one zero-argument word left to say
struct Armed {
    const char* name;
    TaskPriority_t prio;
    sched::TCBStorage* box;
    std::span<std::uint32_t> stack;
    sched::UserTaskWrapper run;

    sched::TCB& spawn_into() const {
        return sched::TCBCreator{
            .name = name,
            .prio = prio,
            .stack = stack,
            .run = run,
        }.spawn_into(*box);
    }
};

template <typename Stage>
struct TaskBuilder {
    Stage stage;

    explicit TaskBuilder(Stage filled) : stage(filled) {}

    // one road per stage: asking for a step from the wrong spot is a
    // compile error, exactly the point of this whole dance
    TaskBuilder<Priorized> prio(TaskPriority_t value) requires std::same_as<Stage, Named> {
        return TaskBuilder<Priorized>{Priorized{stage.name, value}};
    }

    TaskBuilder<Stacked> stack(std::span<std::uint32_t> body) requires std::same_as<Stage, Priorized> {
        return TaskBuilder<Stacked>{Stacked{stage.name, stage.prio, body}};
    }

    // declare the stack SIZE (storage itself comes from the launch arena)
    TaskBuilder<Worded> words(std::size_t count) requires std::same_as<Stage, Priorized> {
        return TaskBuilder<Worded>{Worded{stage.name, stage.prio, count}};
    }

    // the everyday road: hand in a TaskSlot, box and bed travel together
    template <std::size_t Words>
    TaskBuilder<Slotted> stack(TaskSlot<Words>& slot) requires std::same_as<Stage, Priorized> {
        return TaskBuilder<Slotted>{
            Slotted{stage.name, stage.prio, &slot.tcb, slot.stack}};
    }

    // entry carries its argument with it: "an arg without an entry"
    // is not a state the chain can be in
    sched::TCBCreator entry(sched::UserTaskWrapper::UserTask function, void* argument) 
        requires std::same_as<Stage, Stacked> {
        return sched::TCBCreator{
            .name = stage.name,
            .prio = stage.prio,
            .stack = stage.stack,
            .run = {function, argument},
        };
    }

    Launchable entry(sched::UserTaskWrapper::UserTask function, void* argument) 
        requires std::same_as<Stage, Worded> {
        return Launchable{
            .name = stage.name,
            .prio = stage.prio,
            .words = stage.words,
            .run = {function, argument},
        };
    }

    Armed entry(sched::UserTaskWrapper::UserTask function, void* argument) 
        requires std::same_as<Stage, Slotted> {
        return Armed{
            .name = stage.name,
            .prio = stage.prio,
            .box = stage.box,
            .stack = stage.stack,
            .run = {function, argument},
        };
    }
};

// the road starts here
[[nodiscard]] inline TaskBuilder<Named> named(const char* name) {
    return TaskBuilder<Named>{Named{name}};
}

} // namespace ZerOS::task
