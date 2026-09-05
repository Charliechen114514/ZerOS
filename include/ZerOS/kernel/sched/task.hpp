#pragma once

// The task factory: the chain IS the contract. Skip a step and the rest
// of the chain simply does not exist — no comment can save you then.
//   named("blink").prio(1).stack(stack).entry(fn, arg).spawn_into(box)

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

template <typename Stage>
struct TaskBuilder {
    Stage stage;

    explicit TaskBuilder(Stage filled) : stage(filled) {}

    // one road per stage: asking for a step from the wrong spot is a
    // compile error, exactly the point of this whole dance
    TaskBuilder<Priorized> prio(TaskPriority_t value) && requires std::same_as<Stage, Named> {
        return TaskBuilder<Priorized>{Priorized{stage.name, value}};
    }

    TaskBuilder<Stacked> stack(std::span<std::uint32_t> body) && requires std::same_as<Stage, Priorized> {
        return TaskBuilder<Stacked>{Stacked{stage.name, stage.prio, body}};
    }

    // entry carries its argument with it: "an arg without an entry"
    // is not a state the chain can be in
    sched::TCBCreator entry(sched::UserTaskWrapper::UserTask function, void* argument) &&
        requires std::same_as<Stage, Stacked> {
        return sched::TCBCreator{
            .name = stage.name,
            .prio = stage.prio,
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
